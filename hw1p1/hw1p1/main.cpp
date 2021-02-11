/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
// 
// hw1p1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "TestUrlParser.h"

using namespace std;

int main(int argc, char**argv)
{
	if (argc < 2 || argc > 3 ) {
		printf("Incorrect arguments. \nUsage: \n%s %s\nor\n%s <num_threads> <filename.txt>", argv[0], "<url>",argv[0]);
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

	if (argc == 2) {
		string url(argv[1]);
		WebClient client;
		//client.crawl(url);
		//printf("\n############################----x----###############################\n\n\n");
	} else if(argc == 3) {
		
		// check if number of threads is valid
		// Only needed for hw1p2
		/*
		if (atoi(argv[1]) != 1) {
			printf("Incorrect arguments as number of threads should be 1. \nUsage: %s <num_threads> <filename.txt>", argv[0]);
			WSACleanup();
			return 0;
		}*/

		string numThreadsStr(argv[1]);
		int numThreads;
		try {
			numThreads = stoi(numThreadsStr);
		}
		catch (invalid_argument& ex) {
			printf("Incorrect arguments as number of threads should be greater than or equal to 1. \nUsage: %s <num_threads> <filename.txt>", argv[0]);
			WSACleanup();
			return 0;
		}
		
		//read from file
		string filename(argv[2]);

		string content = readFileToString2(filename);
		if (content.empty()) {
			printf("Unable to read any contents from the given file.\n");
			WSACleanup();
			return 0;
		}

		printf("Opened file %s with size %zu\n", filename.c_str(), content.length());
		ThreadManager thredManager;
		thredManager.initProducerConsumer(content, numThreads);
			
	}

	// for unit testing
	//test_urlParser();
	
	
	// call cleanup when done with everything and ready to exit program
	WSACleanup();

	//printf("\n############################----x----###############################\n\n\n");
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

