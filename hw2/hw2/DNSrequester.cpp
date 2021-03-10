#include "pch.h"
#include "DNSrequester.h"
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// not working
string makeDNSquestion(string host) {

	string dot = ".";
	string buf_str = "";
	string part = "";
	int part_len = 0;
	while (true) {
		size_t found = host.find(dot);

		if (found != string::npos) {
			part = host.substr(0, found);
			host = host.substr(found + 1);
		}
		else {
			part = host;
			host = "";
		}
		part_len = part.length();
		buf_str += to_string(part_len);
		buf_str += part;

		if (host.empty()) {
			buf_str += to_string(0);
			break;
		}
	}
	return buf_str;
}

void makeDNSquestion(char* buf, char* host) {
	char dot = '.';

	// point to the host string
	char* myhost = host;
	int rem_len = 0;
	int part_len = 0;
	int i = 0;
	
	while (true) {
		// total length of leftover host string [this will keep reducing as we parse]
		int total_len = strlen(myhost);

		// remaining points to first dot (if any) and to NULL otherwise
		char* remaining = strchr(myhost, dot);
		if (remaining != NULL) {
			// there was a .
			rem_len = strlen(remaining);
			part_len = total_len - rem_len;

			// store part length
			buf[i++] = part_len;

			// copy contents of this part to buf
			memcpy(buf + i, myhost, part_len);
			
			// move i for future updates
			i += part_len;

			//move one step right of the dot
			remaining += 1;

			// host gets reduced as one part and a . is extracted
			myhost = remaining;
		}
		else {
			// no dot was left so the leftover host is the last part
			part_len = total_len;
			
			// copy over the last part
			buf[i++] = part_len;
			memcpy(buf + i, myhost, part_len);
			i = i + part_len;
			
			// we add a 0 at the end
			buf[i] = 0;

			// we are done
			break;
		}
	}
}

void printArr(char* ptr, int len)
{
	cout << endl;
	for (int i = 0; i < len; i++)
		printf("%02x ", (unsigned char)ptr[i]);
	cout << endl<<endl;
}

bool validateResponse(FixedDNSheader* dh, FixedDNSheader* recv_dh) {
	
	// match transaction id between sent and received 
	if (htons(recv_dh->TXID) != htons(dh->TXID)) {
		printf("\t++ invalid reply: TXID mismatch, sent 0x%04x, received 0x%04x\n", htons(dh->TXID), htons(recv_dh->TXID));
		return false;
	}
	
	// checkn Rcode
	int code = htons(recv_dh->flags) & 0xF;
	if (code != 0) {
		printf("\tfailed with Rcode = %d\n", code);
		return false;
	}
	else {
		printf("\tsucceeded with Rcode = 0\n");
	}

	return true;
}

char* jump(char* cur, char* recv_buf, int recv_res) {
	// we have recursively reached the end of the domain name
	if (cur[0] == 0) {
		return cur + 1;
	}

	bool is_compressed = false;
	is_compressed = (unsigned char)cur[0] >= 0xC0;

	if (is_compressed) {
		// 0x3F is 00111111
		// We clear out the left two bits and then consider that byte and the next byte
		// to determine the jump offset (4byte out of which left two bytes are 0s).
		
		// check if next byte exists [next byte is required to find jump offset]
		int index_of_last_byte = recv_res - 1;
		if (cur + 1 - recv_buf > index_of_last_byte) {
			printf("\n\t++invalid record: truncated jump offset\n");
			WSACleanup();
			exit(0);
		}

		int jump_offset = (((unsigned char)cur[0] & 0x3F) << 8) + (unsigned char)cur[1];

		if (jump_offset > recv_res) {
			printf("\n\t++invalid record: jump beyond packet boundary\n");
			WSACleanup();
			exit(0);
		}

		if (jump_offset < 12) {
			printf("\n\t++ invalid record: jump into fixed DNs header\n");
			WSACleanup();
			exit(0);
		}
		
		// make the jump
		char* jumped_ptr = recv_buf + jump_offset;
		char* next = jump(jumped_ptr, recv_buf, recv_res);
		
		/*
		We are at a byte which is >=C0. Next byte is used for offset. These 2 have been processed.
		Now we just need to skip these two and therefore we need to return cur+2
		*/
		return cur + 2;
	}
	else {

		// We are at an uncompressed segment
		
		// how big is this segment?
		int seg_size = cur[0];
		cur = cur + 1;

		if (cur + seg_size - recv_buf > recv_res) {
			printf("\n\t++ invalid record: truncated name\n");
			WSACleanup();
			exit(0);
		}

		char part[MAX_DNS_SIZE];
		memcpy(part, cur, seg_size); // TODO see if this needs to be seg_size+1
		part[seg_size] = '\0';
		printf("%s", part);
		cur = cur + seg_size;
		if (cur[0] != 0) {
			printf(".");
		}
		cur = jump(cur, recv_buf, recv_res);
		return cur;
	}
}

int getQtype(FixedRR* fixed_rr) {
	return (int)htons(fixed_rr->q_type);
}

string getReverseIPfromInput(string lookup) {
	// assume lookup is a.b.c.d
	
	stack<string> st;
	string temp = lookup;

	// extract and push a,b,c to stack to get [c,b,a]
	while (!temp.empty()) {
		size_t found = temp.find(".");
		if (found != string::npos) {
			string part = temp.substr(0,found);
			temp = temp.substr(found + 1);
			st.push(part);
		}
		else {
			break;
		}
	}
	
	// the least significant byte d gets pushed now to get [d,c,b,a]
	st.push(temp);

	// pop and reconstruct string to get d.c.b.a
	string ans = "";
	while (!st.empty()) {
		string part = st.top();
		st.pop();
		if (st.size() > 0) {
			ans = ans + part + ".";
		}
		else {
			ans = ans + part;
		}
	}
	ans = ans + ".in-addr.arpa";

	return ans;
}

bool DNSrequester::makeDNSrequest(std::string lookup, std::string dns_ip)
{
	int type;
	string query;

	hrc::time_point st;
	hrc::time_point en;
	double elapsed;

	// Decide query type: is it a hostnames or an IP address
	DWORD IP = inet_addr(lookup.c_str());
	if (IP == INADDR_NONE)
	{
		// lookup is a hostname => Query type is A
		type = DNS_A;
		query = lookup;
	}
	else {
		// lookup  is an IP => Query type is PTR
		type = DNS_PTR;
		query = getReverseIPfromInput(lookup);

	}

	int packet_size = query.length() + 2 + sizeof(FixedDNSheader) + sizeof(QueryHeader);
	char* buf = new char[packet_size];

	FixedDNSheader* dh = (FixedDNSheader*)buf;
	QueryHeader* qh = (QueryHeader*)(buf + packet_size - sizeof(QueryHeader));
	
	dh->TXID = htons(1);
	dh->flags = htons(DNS_QUERY | DNS_RD | DNS_STDQUERY);
	dh->n_questions = htons(1);
	dh->n_answers = htons(0);
	dh->n_authority = htons(0);
	dh->n_additional = htons(0);

	if (type == DNS_A) {
		qh->q_type = htons(DNS_A);
	}
	else {
		qh->q_type = htons(DNS_PTR);
	}
	qh->q_class = htons(DNS_INET);
	
	/*string question = makeDNSquestion(lookup);
	char* temp = buf + sizeof(FixedDNSheader);
	strcpy_s(temp, int(question.length())+1, question.c_str());*/

	char* host = new char[query.length() + 1];
	strcpy_s(host, query.length() + 1, query.c_str());
	
	makeDNSquestion((char*)(dh+1), host);

	printf("Lookup\t:  %s\n", lookup.c_str());
	printf("Query\t:  %s, type:%d, TXID 0x%04x\n", query.c_str(), type, ntohs(dh->TXID));
	printf("Server\t:  %s\n", dns_ip.c_str());
	printf("*********************************\n");

	int attempt = 0;
	while (attempt < MAX_ATTEMPTS) {
		printf("Attempt %d with %d bytes...", attempt++, packet_size);
		// prepare to send request to server

		// wrapper on system's socket class
		SOCKET sock; // socket handle
		
		// open a UDP socket
		
		// DNS uses UDP
		sock = socket(AF_INET, SOCK_DGRAM, 0);

		if (sock == INVALID_SOCKET) {
			printf("failed as socket open generated error %d\n", WSAGetLastError());
			delete[] buf;
			return false;
		}

		struct sockaddr_in local;
		memset(&local, 0, sizeof(local));
		local.sin_family = AF_INET;
		local.sin_addr.s_addr = INADDR_ANY;
		local.sin_port = htons(0);

		if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
			printf("failed as socket bind generated error %d\n", WSAGetLastError());
			delete[] buf;
			return false;
		}

		struct sockaddr_in remote;
		memset(&remote, 0, sizeof(remote));
		remote.sin_family = AF_INET;
		remote.sin_addr.s_addr = inet_addr(dns_ip.c_str());
		remote.sin_port = htons(53); // DNS port on server

		st = hrc::now();

		// no connect needed, just send
		if (sendto(sock, buf, packet_size, 0, (struct sockaddr*)&remote, int(sizeof(remote))) == SOCKET_ERROR) {
			printf("failed as socket send generated error %d", WSAGetLastError());
			delete[] buf;
			return false;
		}

		// Now prepare to receive
		char* recvBuf = new char[MAX_DNS_SIZE]; // current buffer
		fd_set fd;
		FD_ZERO(&fd);
		FD_SET(sock, &fd);
		timeval tp;
		tp.tv_sec = 10;
		tp.tv_usec = 0;
		struct sockaddr_in response;
		int size = sizeof(response);
		int ret = select(0, &fd, NULL, NULL, &tp);
		int recv_res = 0;
		if (ret > 0) {
			recv_res = recvfrom(sock, recvBuf, MAX_DNS_SIZE, 0, (struct sockaddr*)&response, &size);

			// error processing
			if (recv_res == SOCKET_ERROR) {
				printf("failed on recvfrom with error %d\n", WSAGetLastError());
				delete[] buf;
				delete[] recvBuf;
				return false;
			}
			
			en = hrc::now();        // get end time point
			elapsed = ELAPSED_MS(st, en);
			printf("response in %.2f ms with %d bytes\n", elapsed, recv_res);

			if (response.sin_addr.s_addr != remote.sin_addr.s_addr || response.sin_port != remote.sin_port) {
				printf("failed due to bogus reply\n");
				delete[] buf;
				delete[] recvBuf;
				return false;
			}

			//printArr(recvBuf, recv_res);

			FixedDNSheader* recv_dh = (FixedDNSheader*)recvBuf;
			//TODO handle more corner cases
			if (recv_dh == NULL) {
				printf("failed due to fixed DNS header not being found in resonse\n");
				delete[] buf;
				delete[] recvBuf;
				return false;
			}
			
			printf("\tTXID 0x%04x ", ntohs(recv_dh->TXID));
			printf("flags 0x%04x ", ntohs(recv_dh->flags));
			printf("questions %d ", ntohs(recv_dh->n_questions));
			printf("answers %d ", ntohs(recv_dh->n_answers));
			printf("authority %d ", ntohs(recv_dh->n_authority));
			printf("additional %d", ntohs(recv_dh->n_additional));
			printf("\n");
			
			// Validate response
			if (htons(recv_dh->TXID) != htons(dh->TXID)) {
				printf("\t++ invalid reply: TXID mismatch, sent 0x%04x, received 0x%04x\n", htons(dh->TXID), htons(recv_dh->TXID));
				delete[] buf;
				delete[] recvBuf;
				return false;
			}

			// validate the response
			if (!validateResponse(dh, recv_dh)) {
				return false;
			}

			

			// TODO add support for handling multiple questions
			printf("\t------ [questions] --------\n");
			printf("\t\t");
			char* cur = recvBuf;
			cur = cur + sizeof(FixedDNSheader);
			bool put_dot = false;
			while (true) {
				int part_len = cur[0];
				
				// we have reached end of question
				if (part_len > 0) {
					// point to the first letter of the part
					cur = cur + 1;
					char part[MAX_DNS_SIZE];
					memcpy(part, cur, part_len);
					part[part_len] = '\0';
					
					// This is to avoid putting a dot after the last part e.g., "com"
					if (put_dot)
						printf(".");
					else
						put_dot = true;

					printf("%s", part);
					cur = cur + part_len;

				}
				else{
					// part_len is 0
					cur = cur + 1;
					break;
				}
			}
			
			QueryHeader* recv_qh = (QueryHeader*)cur;
			printf(" type %d class %d\n", htons(recv_qh->q_type), htons(recv_qh->q_class));
			cur = cur + sizeof(QueryHeader);

			u_short num_answers = ntohs(recv_dh->n_answers);
			u_short num_authority = ntohs(recv_dh->n_authority);
			u_short num_additional = ntohs(recv_dh->n_additional);

			vector<u_short> region_count;
			region_count.push_back(num_answers);
			region_count.push_back(num_authority);
			region_count.push_back(num_additional);
			int index = 0;
			map<int, string> mymap;
			mymap[0] = "answers";
			mymap[1] = "authority";
			mymap[2] = "additional";

			for (vector<u_short>::iterator it = region_count.begin(); it != region_count.end(); ++it) {
				u_short count = *it;
				string region = mymap[index++];
				if (count > 0) {
					printf("\t------ [%s] --------\n",region.c_str());
				}
				for (int i = 0;i < count; i++) {

					// cur should not exceed the boundary
					if (cur - recvBuf >= recv_res) {
						printf("\t++ invalid section: not enough records\n");
						delete[] buf;
						delete[] recvBuf;
						return false;
					}

					//printArr(cur, 100);
					printf("\t\t");
					cur = jump(cur, recvBuf, recv_res);
					printf(" ");

					if (cur + sizeof(FixedRR) - recvBuf > recv_res) {
						printf("\t++invalid record: truncated RR answer header\n");
						delete[] buf;
						delete[] recvBuf;
						return false;
					}

					FixedRR* fixed_rr = (FixedRR*)cur;
					// the fixed RR has been extracted so we can move cur forward
					cur = cur + sizeof(FixedRR);
					//printArr(cur, 20);
					int rr_qtype = getQtype(fixed_rr);
					int ttl;

					// validate data length
					if (rr_qtype == DNS_A || rr_qtype == DNS_PTR || rr_qtype == DNS_NS || rr_qtype == DNS_CNAME) {
						u_short data_len;
						data_len = ntohs(fixed_rr->rd_length);
						if (cur + data_len - recvBuf > recv_res) {
							printf("\t++ invalid record: RR value length stretches the answer beyond packet\n");
							delete[] buf;
							delete[] recvBuf;
							return false;
						}
					}

					switch (rr_qtype) {
					case DNS_A:
						printf("A ");
						// Show the data (IP address)
						for (int j = 0;j < 4;j++) {
							int ip_part = (unsigned char)(cur[j] & 0x0F) + 16 * ((unsigned char)cur[j] >> 4);
							printf("%d", ip_part);
							if (j < 3)
								printf(".");
						}

						// The TTL is 4 bytes. Note the use of ntohl which returns 32 bit value. ntohs returns 2 bytes
						ttl = ntohl(fixed_rr->TTL);
						printf(" TTL = %d", ttl);

						// the 4 bytes of ip are not part of FixedRR, so move cur by 4
						cur = cur + 4;

						break;

					case DNS_PTR:
					case DNS_NS:
					case DNS_CNAME:
						if (rr_qtype == DNS_PTR) {
							printf("PTR ");
						}
						else if (rr_qtype == DNS_NS) {
							printf("NS ");
						}
						else {
							printf("CNAME ");
						}
						cur = jump(cur, recvBuf, recv_res);

						// The TTL is 4 bytes. Note the use of ntohl which returns 32 bit value. ntohs returns 2 bytes
						ttl = ntohl(fixed_rr->TTL);
						printf(" TTL = %d", ttl);

						break;

					default:
						break;

					}
					printf("\n");
				}

			}

			// Process answers
			//if (num_answers > 0) {
			//	printf("\t------ [answers] --------\n");
			//}
			//for (int i = 0;i < num_answers;i++) {
			//	
			//	// cur should not exceed the boundary
			//	if (cur - recvBuf >= recv_res) {
			//		printf("\t++ invalid section: not enough records\n");
			//		delete[] buf;
			//		delete[] recvBuf;
			//		return false;
			//	}

			//	//printArr(cur, 100);
			//	printf("\t\t");
			//	cur = jump(cur, recvBuf, recv_res);
			//	printf(" ");

			//	if (cur + sizeof(FixedRR) - recvBuf > recv_res) {
			//		printf("\t++invalid record: truncated RR answer header\n");
			//		delete[] buf;
			//		delete[] recvBuf;
			//		return false;
			//	}
			//	
			//	FixedRR* fixed_rr = (FixedRR*)cur;
			//	// the fixed RR has been extracted so we can move cur forward
			//	cur = cur + sizeof(FixedRR);
			//	//printArr(cur, 20);
			//	int rr_qtype = getQtype(fixed_rr);
			//	int ttl;
			//	
			//	// validate data length
			//	if (rr_qtype == DNS_A || rr_qtype == DNS_PTR || rr_qtype == DNS_NS || rr_qtype == DNS_CNAME) {
			//		u_short data_len;
			//		data_len = ntohs(fixed_rr->rd_length);
			//		if (cur + data_len - recvBuf > recv_res) {
			//			printf("\t++ invalid record: RR value length stretches the answer beyond packet\n");
			//			delete[] buf;
			//			delete[] recvBuf;
			//			return false;
			//		}
			//	}
			//	
			//	switch (rr_qtype) {
			//		case DNS_A:
			//			printf("A ");
			//			// Show the data (IP address)
			//			for (int j = 0;j < 4;j++) {
			//				int ip_part = (unsigned char)(cur[j] & 0x0F) + 16 * ((unsigned char)cur[j] >> 4);
			//				printf("%d", ip_part);
			//				if (j < 3)
			//					printf(".");
			//			}

			//			// The TTL is 4 bytes. Note the use of ntohl which returns 32 bit value. ntohs returns 2 bytes
			//			ttl = ntohl(fixed_rr->TTL);
			//			printf(" TTL = %d",ttl);
			//			
			//			// the 4 bytes of ip are not part of FixedRR, so move cur by 4
			//			cur = cur + 4;
			//			
			//			break;

			//		case DNS_PTR:
			//		case DNS_NS:
			//		case DNS_CNAME:
			//			if (rr_qtype == DNS_PTR) {
			//				printf("PTR ");
			//			}
			//			else if (rr_qtype == DNS_NS) {
			//				printf("NS ");
			//			}
			//			else {
			//				printf("CNAME ");
			//			}
			//			cur = jump(cur, recvBuf, recv_res);
			//			
			//			// The TTL is 4 bytes. Note the use of ntohl which returns 32 bit value. ntohs returns 2 bytes
			//			ttl = ntohl(fixed_rr->TTL);
			//			printf(" TTL = %d", ttl);

			//			break;

			//		default:
			//			break;

			//	}
			//	printf("\n");
			//}

			//// Process authority
			//if (num_authority > 0) {
			//	printf("\t------ [authority] --------\n");
			//}
			//for (int i = 0;i < num_authority;i++) {

			//}

			//// Process additional
			//if (num_additional > 0) {
			//	printf("\t------ [additional] --------\n");
			//}

			//printArr(recvBuf, recv_res);
			delete[] buf;
			delete[] recvBuf;
			return true;
		}
		else if (ret < 0) {
			// error
			printf("failed with Rcode = %d\n", WSAGetLastError());
			delete[] buf;
			delete[] recvBuf;
			return false;
		}
		else {
			// timeout
			en = hrc::now();        // get end time point
			elapsed = ELAPSED_MS(st, en);
			printf("timeout in %d ms\n", int(elapsed));
			continue;
		}
	}

	return true;
}
