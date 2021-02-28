/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
// hw2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

using namespace std;

int main(int argc, char** argv)
{
	if (argc != 3) {
		printf("Incorrect arguments. \nUsage: %s <lookup-string> <DNS-server-IP>\n", argv[0]);
		return 0;
	}

	WSADATA wsaData;

	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	string lookup(argv[1]);
	string dns_server_ip(argv[2]);

	DNSrequester dnsRequester;
	dnsRequester.makeDNSrequest(lookup, dns_server_ip);

	// call cleanup when done with everything and ready to exit program
	WSACleanup();

}