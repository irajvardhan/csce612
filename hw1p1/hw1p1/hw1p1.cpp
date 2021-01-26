// hw1p1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "TestUrlParser.h"

using namespace std;



int main(int argc, char**argv)
{
	if (argc != 2) {
		printf("Incorrect arguments. \nUsage: %s %s\n", argv[0], "<url>");
		return 0;
	}
	
	string url(argv[1]);

	WSADATA wsaData;

	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	// for unit testing
	//test_urlParser();
	client(url);
	
	// call cleanup when done with everything and ready to exit program
	WSACleanup();

	printf("\n############################----x----###############################\n\n\n");
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

