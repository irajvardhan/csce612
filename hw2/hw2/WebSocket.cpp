/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "WebSocket.h"

Socket::Socket() {
	buf = (char*)malloc(INITIAL_BUF_SIZE);
	allocatedSize = INITIAL_BUF_SIZE;
	curPos = 0;
	// sock will be initialized and opened in the Open() method
}

// open a UDP socket
bool Socket::Open(void) {

	// DNS uses UDP
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		printf("failed as socket open generated error %d\n", WSAGetLastError());
		//WSACleanup();
		return false;
	}

	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(0);

	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		printf("failed as socket bind generated error %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool Socket::Send(char* buf, int size, sockaddr_in &remote, int size_remote)
{
	if (sendto(sock, buf, size, 0, (struct sockaddr*)&remote, size_remote) == SOCKET_ERROR) {
		printf("failed as socket send generated error %d", WSAGetLastError());
		return false;
	}
	return true;
}


void Socket::Close(void) {
	closesocket(sock);
	//WSACleanup();
}

Socket::~Socket() {
	free(buf);
	Close();
}