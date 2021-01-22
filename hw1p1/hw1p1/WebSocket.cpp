#include "pch.h"
#include "WebSocket.h"

class  Socket {
	SOCKET sock; // socket handle
	char* buf; // current buffer
	int allocatedSize; //bytes allocated for buf
	int curPos; // extra stuff as needed

	bool Read(void);
};


Socket::Socket() {
	buf = (char*) malloc(INITIAL_BUF_SIZE);
	allocatedSize = INITIAL_BUF_SIZE;
}

bool Socket::Read(void) {
	TIMEVAL timeout;
	timeout.tv_sec = 10; //sec
	timeout.tv_usec = 0; //ms

	// set of socket descriptors
	fd_set fds;
	
	// clear set of descriptors
	FD_ZERO(&fds);

	// add socket descriptor to set
	FD_SET(sock, &fds);
	DWORD ret;

	while (true) {
		ret == select(0, &fds, NULL, NULL, &timeout);
		
		// wait to see if socket has any data
		if (ret > 0) {

			// new data is available; now read the next segment
			int bytes = recv(sock, buf + curPos, allocatedSize - curPos, 0);

			// error
			if (bytes < 0) {
				printf("Receive error: %d\n", WSAGetLastError());
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
					printf("ERROR: realloc failed to increase size of buffer\n");
					break;
				}
				
				buf = tempBuf;
				allocatedSize = newAllocatedSize;
			}

		}
		else if (ret == -1) {
			printf("Error: %d\n", WSAGetLastError());
			break;
		}
		else {
			printf("Waited for %d seconds but timed-out due to no data", timeout.tv_sec);
			break;
		}

	}
	return false;


}