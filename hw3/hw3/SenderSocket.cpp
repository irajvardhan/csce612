#include "pch.h"
#include "SenderSocket.h"
#pragma comment(lib, "ws2_32.lib")

SenderSocket::SenderSocket()
{
	obj_st_time = hrc::now();        // get start time point
	is_conn_open = false;
}

int SenderSocket::Open(char* target_host, int rcv_port, int sender_window, LinkProperties* lp)
{
	// check if Close() hasn't been called after a call to Open()
	if (is_conn_open) {
		return ALREADY_CONNECTED;
	}

	hrc::time_point st;
	hrc::time_point syn_time;
	hrc::time_point cur_time;
	double elapsed;
	
	// open a UDP socket
	// DNS uses UDP
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		cur_time = hrc::now();
		elapsed = ELAPSED(obj_st_time, cur_time);
		printf("[ %.3f ] --> socket open failed with %d\n", elapsed, WSAGetLastError());
		//delete[] buf;
		return INVALID_SOCKET;
	}

	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(0);

	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		cur_time = hrc::now();
		elapsed = ELAPSED(obj_st_time, cur_time);
		printf("[ %.3f ] --> socket bind failed with %d\n", elapsed, WSAGetLastError());
		//delete[] buf;
		return BIND_SOCKET_FAILED;
	}
	
	struct hostent* remote;
	// first assume that the string is an IP address
	DWORD IP = inet_addr(target_host);
	if (IP == INADDR_NONE)
	{
		// if this is not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(target_host)) == NULL)
		{
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			printf("[ %.3f ] --> target %s is invalid \n", elapsed, target_host);
			return INVALID_NAME;
		}
		else // take the first IP address and copy into sin_addr
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
	}
	else
	{
		// if this is a valid IP, directly drop its binary version into sin_addr
		server.sin_addr.S_un.S_addr = IP;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(rcv_port);

	float rto = 1.0;
	my_lp = *lp;
	
	SenderSynHeader ss_hdr;
	ss_hdr.lp = *lp;
	ss_hdr.lp.bufferSize = sender_window; //TODO: should we add +R as per instructions? What is R?
	
	/*For the SYN phase, the RTO starts from a value of 1 second.*/
	ss_hdr.sdh.flags.SYN = 1;
	ss_hdr.sdh.seq = 0;

	int count = 1;
	while (count <= MAX_ATTEMPTS_SYN) {
		//st = obj_st_time;
		syn_time = hrc::now();
		elapsed = ELAPSED(obj_st_time, syn_time);
		
		printf("[ %.3f ] --> SYN %d (attempt %d of %d, RTO %.3f) to %s\n", elapsed, ss_hdr.sdh.seq, count, MAX_ATTEMPTS_SYN, rto, inet_ntoa(server.sin_addr));
		char* header_buf = (char*)&ss_hdr;
		// no connect needed, just send
		if (sendto(sock, header_buf, sizeof(ss_hdr), 0, (struct sockaddr*)&server, int(sizeof(server))) == SOCKET_ERROR) {
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			printf("[ %.3f ] --> failed sendto with %d\n", elapsed, WSAGetLastError());
			//delete[] buf;
			return FAILED_SEND;
		}
		

		// Now prepare to receive
		fd_set fd;
		FD_ZERO(&fd);
		FD_SET(sock, &fd);
		timeval tp;
		struct sockaddr_in response;
		int size = sizeof(response);

		tp.tv_sec = (long)rto;
		tp.tv_usec = (rto - tp.tv_sec) * 1000000;

		int ret = select(0, &fd, NULL, NULL, &tp);
		int recv_res = 0; // number of bytes received

		char* recv_buf = new char[sizeof(ReceiverHeader)];
		
		if (ret > 0) {
			recv_res = recvfrom(sock, recv_buf, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size);

			// error processing
			if (recv_res == SOCKET_ERROR) {
				cur_time = hrc::now();
				elapsed = ELAPSED(obj_st_time, cur_time);
				printf("[ %.3f ] <-- failed recvfrom with %d\n", elapsed, WSAGetLastError());
				//delete[] buf;
				//delete[] recvBuf;
				return FAILED_RECV;
			}

			ReceiverHeader* rcv_hdr = (ReceiverHeader*)recv_buf;

			/*
			* If we dont get back a SYN ACK then it means something went wrong. Try again.
			* This is less likely though.
			*/
			if (rcv_hdr->flags.SYN != 1 or rcv_hdr->flags.ACK!=1) {
				count += 1;
				continue;
			}


			hrc::time_point syn_ack_time = hrc::now();        // get end time point
			
			// time elapsed since constructor of this class was called
			elapsed = ELAPSED(obj_st_time, syn_ack_time); // in seconds
			
			// 3 times the time elapsed between sending the SYN and receiving a SYN-ACK
			rto = ELAPSED(syn_time, syn_ack_time) * 3;
			
			// This will be used in sending FIN
			my_rto = rto;

			printf("[ %.3f ] <-- SYN-ACK %d window %d; setting initial RTO to %.3f\n", elapsed, rcv_hdr->ackSeq, rcv_hdr->recvWnd, rto);

			// We mark the connection open on verifying the SYN-ACK received
			is_conn_open = true;

			return STATUS_OK;

		}
		else if (ret < 0) {
			// ERROR
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			printf("[ %.3f ] <-- failed with %d\n", elapsed, WSAGetLastError());
			return FAILED_OTHER;
		}
		else {
			//ret is 0
			count += 1;
		}
	}

    return TIMEOUT;
}

int SenderSocket::Close()
{
	if (!is_conn_open) {
		return NOT_CONNECTED;
	}

	hrc::time_point st;
	hrc::time_point fin_time;
	hrc::time_point cur_time;
	double elapsed;
	SenderSynHeader ss_hdr;
	ss_hdr.lp = my_lp;
	
	/*For the FIN phase, the RTO starts from the RTO derived during SYN phase.*/
	float rto = my_rto;

	ss_hdr.sdh.flags.FIN = 1;
	ss_hdr.sdh.seq = 0;

	int count = 1;
	while (count <= MAX_ATTEMPTS_OTHER) {
		//st = obj_st_time;
		fin_time = hrc::now();
		elapsed = ELAPSED(obj_st_time, fin_time);

		printf("[ %.3f ] --> FIN %d (attempt %d of %d, RTO %.3f) to %s\n", elapsed, ss_hdr.sdh.seq, count, MAX_ATTEMPTS_OTHER, rto, inet_ntoa(server.sin_addr));
		char* header_buf = (char*)&ss_hdr;
		// no connect needed, just send
		if (sendto(sock, header_buf, sizeof(ss_hdr), 0, (struct sockaddr*)&server, int(sizeof(server))) == SOCKET_ERROR) {
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			printf("[ %.3f ] --> failed sendto with %d\n", elapsed, WSAGetLastError());
			//delete[] buf;
			return FAILED_SEND;
		}


		// Now prepare to receive
		fd_set fd;
		FD_ZERO(&fd);
		FD_SET(sock, &fd);
		timeval tp;
		struct sockaddr_in response;
		int size = sizeof(response);

		tp.tv_sec = (long)rto;
		tp.tv_usec = (rto - tp.tv_sec) * 1000000;

		int ret = select(0, &fd, NULL, NULL, &tp);
		int recv_res = 0; // number of bytes received

		char* recv_buf = new char[sizeof(ReceiverHeader)];

		if (ret > 0) {
			recv_res = recvfrom(sock, recv_buf, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size);

			// error processing
			if (recv_res == SOCKET_ERROR) {
				cur_time = hrc::now();
				elapsed = ELAPSED(obj_st_time, cur_time);
				printf("[ %.3f ] <-- failed recvfrom with %d\n", elapsed, WSAGetLastError());
				//delete[] buf;
				//delete[] recvBuf;
				return FAILED_RECV;
			}

			ReceiverHeader* rcv_hdr = (ReceiverHeader*)recv_buf;

			/*
			* If we dont get back a FIN ACK then it means something went wrong. Try again.
			* This is less likely though.
			*/
			if (rcv_hdr->flags.FIN != 1 or rcv_hdr->flags.ACK != 1) {
				count += 1;
				continue;
			}

			hrc::time_point fin_ack_time = hrc::now();        // get end time point

			// time elapsed since constructor of this class was called
			elapsed = ELAPSED(obj_st_time, fin_ack_time); // in seconds

			printf("[ %.3f ] <-- FIN-ACK %d window %d\n", elapsed, rcv_hdr->ackSeq, rcv_hdr->recvWnd);
			
			// The connection can now be considered closed
			is_conn_open = false;
			
			return STATUS_OK;

		}
		else if (ret < 0) {
			// ERROR
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			printf("[ %.3f ] <-- failed with %d\n", elapsed, WSAGetLastError());
			return FAILED_OTHER;
		}
		else {
			//ret is 0
			count += 1;
		}
	}

	return TIMEOUT;
}

int SenderSocket::Send(char* sendBuf, int numBytes)
{

	return 0;
}
