#include "pch.h"
#include "WebSocket.h"

Socket::Socket() {
	buf = (char*) malloc(INITIAL_BUF_SIZE);
	allocatedSize = INITIAL_BUF_SIZE;
	curPos = 0;
	// sock will be initialized and opened in the Open() method
}

// open a TCP socket
bool Socket::Open(void) {
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET){
		//printf("socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	return true;
}

bool Socket::Connect(struct sockaddr_in server) {
	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR){
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
bool Socket::Read(void) {
	// set of socket descriptors
	fd_set fds;

	TIMEVAL timeout;

	int ret;

	while (true) {
		
		// clear set of descriptors
		FD_ZERO(&fds);

		// add socket descriptor to set
		FD_SET(sock, &fds);
		
		timeout.tv_sec = 10; //sec
		timeout.tv_usec = 0; //ms

		// wait to see if socket has any data
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
				return true;
			}

			curPos += bytes;

			// check if we need to resize the buffer
			if (allocatedSize - curPos < REMAINING_SPACE_THRESHOLD) {
				//  double the allocation size
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
			//printf("Timeout occurred. Waited for %d seconds but received no data\n", timeout.tv_sec);
			break;
		}
	}
	return false;


}

void Socket::Close(void) {
	closesocket(sock);
}

char* Socket::GetBufferData() {
	return buf;
}

Socket::~Socket() {
	free(buf);
	Close();
}