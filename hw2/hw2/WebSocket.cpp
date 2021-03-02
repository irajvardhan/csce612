/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "WebSocket.h"

Socket::Socket() {
	buf = (char*)malloc(MAX_DNS_SIZE);
	allocatedSize = MAX_DNS_SIZE;
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


bool Socket::Read(struct sockaddr_in &remote)
{
	fd_set fd;
	FD_ZERO(&fd);
	FD_SET(sock, &fd);
	timeval tp;
	tp.tv_sec = 10;
	tp.tv_usec = 0;
	struct sockaddr_in response;
	int size = sizeof(response);
	int ret = select(0, &fd, NULL, NULL, &tp);
	int recv_res = 0;
	if (ret > 0) {
		recv_res = recvfrom(sock, buf, MAX_DNS_SIZE, 0, (struct sockaddr*)&response, &size);

		// error processing
		if (recv_res == SOCKET_ERROR) {
			printf("failed on recvfrom with error %d\n", WSAGetLastError());
			return false;
		}

		if (response.sin_addr.s_addr != remote.sin_addr.s_addr || response.sin_port != remote.sin_port) {
			printf("failed due to bogus reply\n");
			return false;
		}
	}

}

void Socket::Close(void) {
	closesocket(sock);
}

char* Socket::GetBufferData(void)
{
	return buf;
}

Socket::~Socket() {
	free(buf);
	Close();
}