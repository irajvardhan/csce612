/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "WebSocket.h"

std::mutex mutex_sock;

Socket::Socket() {
	buf = (char*) malloc(INITIAL_BUF_SIZE);
	allocatedSize = INITIAL_BUF_SIZE;
	curPos = 0;
	// sock will be initialized and opened in the Open() method
}

// open a TCP socket
bool Socket::Open(void) {
	
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	

	if (sock == INVALID_SOCKET) {
		//printf("socket() generated error %d\n", WSAGetLastError());
		//WSACleanup();
		return false;
	}
	return true;
}

bool Socket::Connect(struct sockaddr_in server) {
	
	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		//printf("Connection error: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool Socket::Send(char* sendBuf, int requestLen) {
	if (send(sock, sendBuf, requestLen, 0) == SOCKET_ERROR) {
		//printf("Send error: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

// read from a socket
RecvOutcome Socket::Read(int maxDownloadSize) {
	// set of socket descriptors
	fd_set fds;

	TIMEVAL timeout;
	timeout.tv_sec = 10; //sec
	timeout.tv_usec = 0; //ms
	int ret;
	RecvOutcome outcome;

	hrc::time_point st;
	hrc::time_point en;
	double elapsed;
	st = hrc::now();        // get start time point

	while (true) {
		en = hrc::now();        // get end time point
		elapsed = ELAPSED_MS(st, en);
		if (elapsed > MAXELAPSEDTIME) {
			//printf("failed with slow download\n");
			outcome.errorAlreadyShown = true;
			outcome.success = false;
			break;
		}

		// clear set of descriptors [not necessary in loop but good practice]
		FD_ZERO(&fds);

		// add socket descriptor to set 
		// When dealing with multiple sockets, the call to select will only leave a subset of sockets
		// in the fd_set. So need to put all the sockets back to fds.
		FD_SET(sock, &fds);
		
		// wait to see if socket has any data
		// This is a blocking call. It returns when data is received or on timeout
		if ((ret = select(0, &fds, NULL, NULL, &timeout)) > 0) {

			// new data is available; now read the next segment
			int bytes = recv(sock, buf + curPos, allocatedSize - curPos, 0);

			// error
			if (bytes < 0) {
				//printf("Receive error: %d\n", WSAGetLastError());
				break;
			}

			// connection closed
			if (bytes == 0) {
				// NULL terminate the buffer
				buf[curPos] = '\0';
				outcome.success = true;
				outcome.contentSize = curPos + 1;
				return outcome;
			}

			curPos += bytes;

			// check if we need to resize the buffer
			if (allocatedSize - curPos < REMAINING_SPACE_THRESHOLD) {
				//  double the allocation size
				if (curPos >= maxDownloadSize) {
					//printf("failed with exceeding max\n");
					outcome.errorAlreadyShown = true;
					outcome.success = false;
					break;
				}
				
				
				int newAllocatedSize = allocatedSize << 1; 
				char* tempBuf = (char*)realloc(buf, newAllocatedSize);
				
				// realloc failed
				if (tempBuf == NULL) {
					//printf("ERROR: realloc failed to increase size of buffer\n");
					break;
				}
				
				buf = tempBuf;
				allocatedSize = newAllocatedSize;
			}

		}
		
		else if (ret == -1) {
			//printf("Error: %d\n", WSAGetLastError());
			break;
		}
		else {
			//printf("failed with slow download\n");
			outcome.errorAlreadyShown = true;
			outcome.success = false;
			break;
		}
	}
	return outcome;


}

void Socket::Close(void) {
	closesocket(sock);
	//WSACleanup();
}

char* Socket::GetBufferData() {
	return buf;
}

Socket::~Socket() {
	free(buf);
	Close();
}