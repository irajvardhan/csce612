/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "DNSrequester.h"
#pragma comment(lib, "ws2_32.lib")

using namespace std;

/* 
* global variable to recursively construct a name 
* that is part of an RR.
* We may need to make jumps to construct this name.
* Also, a name may be corrupted if the packet is malformed.
*/
string name;

/*
* We use the below map to keep track of jump offsets that have
* been seen. This is essentially to detect jump loops.
* We clear this map each time we are calling the jump function.
*/
map<int, bool> offset_seen;
map<int, bool>::iterator m_it;

/* 
* This function did not work
*/
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

/*
* Construct the DNS question is the required format
*/
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

/*
* This is a helper method.
* It Prints the content of the buffer in hex.
* Output is same as what can be seen in Wireshark.
* return: None
*/
void printArr(char* ptr, int len)
{
	cout << endl;
	for (int i = 0; i < len; i++)
		printf("%02x ", (unsigned char)ptr[i]);
	cout << endl<<endl;
}

/*
* Validates the response of a DNS request
* 1. Matches transaction ID
* 2. Checks Rcode
* return: True if response is valid else False
*/
bool validateResponse(FixedDNSheader* dh, FixedDNSheader* recv_dh) {
	
	// match transaction id between sent and received 
	if (ntohs(recv_dh->TXID) != ntohs(dh->TXID)) {
		printf("\t++ invalid reply: TXID mismatch, sent 0x%04x, received 0x%04x\n", ntohs(dh->TXID), ntohs(recv_dh->TXID));
		return false;
	}
	
	// check Rcode
	int code = ntohs(recv_dh->flags) & 0xF;
	if (code != 0) {
		printf("\tfailed with Rcode = %d\n", code);
		return false;
	}
	else {
		printf("\tsucceeded with Rcode = 0\n");
	}

	return true;
}

/*
* This function processes a DNS name which could be compressed. In such cases
* it needs to jump to locations pointed to by the 14-bit metadata.
* We accumulate the name in a global variable.
* 
* return: pointer to byte following the name
* 
*/
char* jump(char* cur, char* recv_buf, int recv_res) {
	// we have recursively reached the end of the domain name
	if (cur[0] == 0) {
		return cur + 1;
	}

	/*
	* We have arrived at a compressed part of an answer 
	* if the current byte has a value greater than 0xC0
	* i.e. its first two bits will be 11......
	* The remaining 6bits and the next 8 bits will be used
	* to determine the jump offset
	*/
	bool is_compressed = false;
	is_compressed = (unsigned char)cur[0] >= 0xC0; // 0xC0 = 1100 0000 in binary

	if (is_compressed) {
		// 0x3F is 00111111
		// We clear out the left two bits and then consider that byte and the next byte
		// to determine the jump offset (4byte out of which left two bytes are 0s).
		
		// check if next byte exists [next byte is required to find jump offset]
		int index_of_last_byte = recv_res - 1;
		if (cur + 1 - recv_buf > index_of_last_byte) {
			printf("\n\t\t++invalid record: truncated jump offset\n");
			WSACleanup();
			exit(0);
		}

		/*
		* We need a jump offset of two bytes (B1 B2). Byte1 is obtained by taking & with the mask
		* 0x3F which is 0011 1111. By doing so, we exclude the leftmost two bits of first byte.
		* We left shift this byte by 8 to obtain byte1. Byte2 is simply the following byte cur[1].
		*/
		int jump_offset = (((unsigned char)cur[0] & 0x3F) << 8) + (unsigned char)cur[1];

		if (jump_offset > recv_res) {
			printf("\n\t\t++invalid record: jump beyond packet boundary\n");
			WSACleanup();
			exit(0);
		}

		if (jump_offset < 12) {
			printf("\n\t\t++ invalid record: jump into fixed DNS header\n");
			WSACleanup();
			exit(0);
		}
		
		/*
		* --Error condition--
		* Check if there is a loop in jumping
		* 
		*/
		m_it = offset_seen.find(jump_offset);
		if (m_it != offset_seen.end()) {
			printf("\n\t\t++ invalid record: jump loop\n");
			WSACleanup();
			exit(0);
		}
		else {
			offset_seen[jump_offset] = true;
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
			printf("\n\t\t++ invalid record: truncated name\n");
			WSACleanup();
			exit(0);
		}

		char part[MAX_DNS_SIZE];
		memcpy(part, cur, seg_size); // TODO see if this needs to be seg_size+1
		part[seg_size] = '\0';
		name += string(part);
		//printf("%s", part);
		cur = cur + seg_size;
		if (cur[0] != 0) {
			//printf(".");
			name += ".";
		}
		cur = jump(cur, recv_buf, recv_res);
		return cur;
	}
}

/*
* return: question type
*/
int getQtype(FixedRR* fixed_rr) {
	return (int)htons(fixed_rr->q_type);
}

/*
* return: a string containing the reversed IP along with .in-addr.arpa appended to it
*/
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

/*
* This function makes the DNS request and parses the response 
* utilizing other helper methods.
* 
* returns: true if successful else false
*/
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
		remote.sin_addr.s_addr = inet_addr(dns_ip.c_str()); // dns_ip is the IP of the local DNS server we are connecting to
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
		int recv_res = 0; // number of bytes received
		
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

			// Check if response is smaller than FixedDNSheader which is of 12 bytes
			if (recv_res < 12) {
				printf("\t++ invalid reply: packet smaller than fixed DNS header.\n");
				delete[] buf;
				delete[] recvBuf;
				return false;
			}

			// Extract first 12 bytes of information from the response buffer. This will get loaded into the structure specified.
			FixedDNSheader* recv_dh = (FixedDNSheader*)recvBuf;
			//TODO handle more corner cases. Also, this check may not be required.
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
			
			// validate the response
			if (!validateResponse(dh, recv_dh)) {
				delete[] buf;
				delete[] recvBuf;
				return false;
			}

			char* cur = recvBuf;
			cur = cur + sizeof(FixedDNSheader); // cur is now pointing to the first byte after the FixedDNSheader

			// Note that the query-name part of questions is not compressed [no jump needed]
			printf("\t------ [questions] --------\n");
			for (int q=0; q< ntohs(recv_dh->n_questions);q++){
				printf("\t\t");
				bool put_dot = false;
				while (true) {
					int part_len = cur[0];
				
					// we have to process a part of the question
					if (part_len > 0) {
						// point to the first letter of the part
						cur = cur + 1;
						char part[MAX_DNS_SIZE];
						memcpy(part, cur, part_len);
						part[part_len] = '\0';
					
						// print a dot only if a part has been printer before
						// This is to avoid putting a dot before the first part 
						// e.g., "shop.amazon.com should NOT print as .shop.amazon.com"
						if (put_dot)
							printf(".");
						else
							put_dot = true;

						printf("%s", part);
						cur = cur + part_len;

					}
					else{
						// part_len is 0
						cur = cur + 1; // cur now points to the QueryHeader
						break;
					}
				}
				QueryHeader* recv_qh = (QueryHeader*)cur;
				printf(" type %d class %d\n", ntohs(recv_qh->q_type), ntohs(recv_qh->q_class));
				cur = cur + sizeof(QueryHeader); // if there is another question, cur will point to the beginning of that, otherwise to the next component (e.g., an answer)
			}

			

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

					printf("\t\t");
					name = "";
					offset_seen.clear();
					cur = jump(cur, recvBuf, recv_res);
					printf("%s", name.c_str());
					printf(" ");

					/*
					* --Error Condition--
					* We have processed the name. Now we need to extract FixedRR which of 10 bytes 
					* comprising type (2 bytes), class (2 bytes), TTL (4 bytes), and Rdata length (2 bytes).
					* If we dont have 10 bytes available to read, it means the RR answer header is truncated
					* and the response packet is malformed.
					*/
					if (cur + sizeof(FixedRR) - recvBuf > recv_res) {
						printf("\n\t++ invalid record: truncated RR answer header\n");
						delete[] buf;
						delete[] recvBuf;
						return false;
					}

					FixedRR* fixed_rr = (FixedRR*)cur;
					// the fixed RR has been extracted so we can move cur forward
					cur = cur + sizeof(FixedRR);
					int rr_qtype = getQtype(fixed_rr);
					int ttl;

					// validate data length
					if (rr_qtype == DNS_A || rr_qtype == DNS_PTR || rr_qtype == DNS_NS || rr_qtype == DNS_CNAME) {
						u_short data_len;
						data_len = ntohs(fixed_rr->rd_length);
						if (cur + data_len - recvBuf > recv_res) {
							printf("\n\t++ invalid record: RR value length stretches the answer beyond packet\n");
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
						name = "";
						offset_seen.clear();
						cur = jump(cur, recvBuf, recv_res);
						printf("%s", name.c_str());

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
