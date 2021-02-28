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

bool DNSrequester::makeDNSrequest(std::string lookup, std::string dns_ip)
{
	int type;
	string query;

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
		query = lookup; //TOOD COMPUTE IP STRING HERE
	}

	int packet_size = lookup.length() + 2 + sizeof(FixedDNSheader) + sizeof(QueryHeader);
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

	char* host = new char[lookup.length() + 1];
	strcpy_s(host, lookup.length() + 1, lookup.c_str());
	
	makeDNSquestion((char*)(dh+1), host);

	for (int i = 0;i < packet_size;i++) {
		cout << buf[i];
	}
	cout << endl;

	printf("Lookup\t:  %s\n", lookup.c_str());
	printf("Query\t:  %s, type:%d, TXID 0x%04x\n", lookup.c_str(), type, ntohs(dh->TXID));
	printf("Server\t:  %s\n", dns_ip.c_str());
	printf("*********************************\n");

	int attempt = 0;
	while (attempt < MAX_ATTEMPTS) {
		printf("Attempt %d with %d bytes...", attempt, packet_size);
		
		// wrapper on system's socket class
		Socket webSocket;
		// open a UDP socket
		if (!webSocket.Open()) {
			delete[] buf;
			return false;
		}

		struct sockaddr_in remote;
		memset(&remote, 0, sizeof(remote));
		remote.sin_family = AF_INET;
		remote.sin_addr.s_addr = inet_addr(dns_ip.c_str());
		remote.sin_port = htons(53); // DNS port on server

		if (!webSocket.Send(buf, packet_size, remote, int(sizeof(remote)))) {
			delete[] buf;
			return false;
		}




		attempt++;
	}

	return true;
}
