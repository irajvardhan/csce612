/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#pragma once

#define INITIAL_BUF_SIZE 4096
#define REMAINING_SPACE_THRESHOLD 1024
#define MAXELAPSEDTIME 10000 //ms

#include"pch.h"

typedef struct recvOutcome {
	bool errorAlreadyShown;
	bool success;
	int contentSize;

	recvOutcome() {
		errorAlreadyShown = false;
		success = false;
	}
}RecvOutcome;

class Socket {
	SOCKET sock; // socket handle
	char* buf; // current buffer
	int allocatedSize; //bytes allocated for buf
	int curPos; // extra stuff as needed

public:
	Socket();
	bool Open(void);
	bool Send(char* buf, int size, struct sockaddr_in&remote, int size_remote);
	//RecvOutcome Read(int maxDownloadSize);
	bool Read(struct sockaddr_in& remote);
	void Close(void);
	char* GetBufferData(void);
	~Socket();
};