/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#pragma once

#define INITIAL_BUF_SIZE 4096
#define REMAINING_SPACE_THRESHOLD 1024
#define MAXELAPSEDTIME 10000 //ms

typedef struct recvOutcome {
	bool errorAlreadyShown;
	bool success;

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
	bool Connect(struct sockaddr_in server);
	bool Send(char* sendBuf, int requestLen);
	RecvOutcome Read(int maxDownloadSize);
	void Close(void);
	char* GetBufferData(void);
	~Socket();
};