// hw1p1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "TestUrlParser.h"

using namespace std;

void client(void);

int main(int argc, char**argv)
{
    cout << "Starting crawler\n";

	WSADATA wsaData;

	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	test_urlParser();


	//client();

	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

