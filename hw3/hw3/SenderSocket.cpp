/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "SenderSocket.h"
#pragma comment(lib, "ws2_32.lib")

using namespace std;

condition_variable cv;
mutex mtx;
bool statDone = false;
float alpha = 0.125; // used for RTO computation
float beta = 0.25;

bool SenderSocket::debug_mode() {
	int r = rand() % 100000;
	if (r == 0) {
		return false;
	}
	else {
		return false;
	}
}

bool cond_check() {
	return statDone == true;
}

DWORD WINAPI showStats(LPVOID lpParams) {

	SharedParameters* params = (SharedParameters*)lpParams;
	hrc::time_point cur_time;
	double elapsed;
	double elapsed_interval;
	unique_lock<mutex> lck(mtx);

	int prev_base = 0;

	hrc::time_point prev_time = params->obj_st_time;

	int count_of_same = 0;

	//while (cv.wait_for(lck, 2s, cond_check) == false) {
	while (WaitForSingleObject(params->event_quit, 2000) == WAIT_TIMEOUT) {
		cur_time = hrc::now();
		elapsed = ELAPSED(params->obj_st_time, cur_time);
		int num_bits_received_interval = (params->base - prev_base) * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) * 8;
		
		prev_base = params->base;
		elapsed_interval = ELAPSED(prev_time, cur_time);
		prev_time = cur_time;
		
		//todo check if we need to wrap this with mutex
		WaitForSingleObject(params->mtx, INFINITE);
		params->speed = (num_bits_received_interval/1e6) / elapsed_interval; // in Mbps
		ReleaseMutex(params->mtx);

		
		//  B 18 ( 0.0 MB) N 19 T 0 F 0 W 1 S 0.105 Mbps RTT 0.102
		//printf("[%4d] B %8d ( %4.1f MB) N %8d T %4d F%4d W %4d S %.3f Mbps RTT %.3f | devRTT: %.3f timeout: %d\n", int(elapsed), params->base, params->MBacked, params->nextSeq,
		//	params->T, params->F, params->windowSize, params->speed,
		//	params->RTT
		//	,params->devRTT, params->temp_rto //remove this
		//);

		printf("[%4d] B %8d ( %5.1f MB) N %8d T %4d F%4d W %4d S %4.3f Mbps RTT %.3f\n", int(elapsed), params->base, params->MBacked, params->nextSeq,
			params->T, params->F, params->windowSize, params->speed,
			params->RTT
		);
	}
	
	//printf("stats thread ended\n");

	return 0;

}

DWORD WINAPI runWorker(LPVOID lpParams) {

	SenderSocket* ss = (SenderSocket*)lpParams;

	/*
	* the UDP sender/receiver buffer inside the Windows kernel is configured to support
	only 8 KB of unprocessed data. You can achieve higher outbound performance and prevent
	packet loss in the inbound direction by increasing both buffers,
	*/
	int kernel_buffer = 20e6;
	if (setsockopt(ss->sock, SOL_SOCKET, SO_RCVBUF, (char*)&kernel_buffer, sizeof(int)) == SOCKET_ERROR) {
		printf("Failed to increase inbound kernel buffer with %d", WSAGetLastError());
	}

	kernel_buffer = 20e6; //20 meg
	if (setsockopt(ss->sock, SOL_SOCKET, SO_SNDBUF, (char*)&kernel_buffer, sizeof(int)) == SOCKET_ERROR) {
		printf("Failed to increase outbound kernel buffer with %d", WSAGetLastError());
	}

	// then setting your worker thread to time - critical priority
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	// Associate the socket receive ready event with the socket (sock)
	if (WSAEventSelect(ss->sock, ss->sock_recv_ready, FD_READ) == SOCKET_ERROR)
	{
		printf("WSAEventSelect failed with error %d\n", WSAGetLastError());
		return 0;
	}

	//HANDLE arr_handles[] = { ss->sock_recv_ready, ss->full}; // todo check if ss->params.event_quit is needed
	HANDLE arr_handles[] = { ss->sock_recv_ready, ss->full, ss->params.worker_quit }; // todo ss->params.event_quit

	DWORD timeout;

	// This will be incremented by worked thread from here onwards
	int next_to_send = ss->params.base;

	while (true) {
		// todo
		/*if (pending packets)
			timeout = timerExpire - cur_time;
		else*/

		// TODO find out what should be the condition here to check for pending packets
		if (ss->params.nextSeq > 1) {
			timeout = 1000*ss->my_rto; //my_rto is in sec so we need to convert to ms as expecte by WaitForMultipleObjects
			ss->params.temp_rto = timeout;
		}
		else {
			timeout = INFINITE;
		}
		
		//int signal = WaitForMultipleObjects(2, arr_handles, false, timeout);
		int signal = WaitForMultipleObjects(3, arr_handles, false, timeout);

		if (signal == WAIT_OBJECT_0) {
			WSAEnumNetworkEvents(ss->sock, ss->sock_recv_ready, &ss->nw_events); // Without this line ss->ReceiveACK() keeps getting called
			ss->ReceiveACK();
		}
		else if (signal == WAIT_OBJECT_0 + 1) {
			int res = ss->SendToUtil(next_to_send);
			next_to_send++;

			//todo check if this should be done here or anywhere else
			// todo check if mutex needed

			WaitForSingleObject(ss->params.mtx, INFINITE);
			ss->params.nextSeq += 1;
			ReleaseMutex(ss->params.mtx);

		}
		else if (signal == WAIT_TIMEOUT) {
			//if (ss->debug_mode()) 
				//printf("WAIT_TIMEOUT condition of runWorker\n");
			// a timeout occurred 

			// TODO check if this is the right way of knowing if we are done sending everything
			if (ss->params.populating_complete and ss->sndBase == ss->seq) {
				return 1;
			}

			// check if we are under the threshold of attempts
			if (ss->pkt_num_attempts[ss->sndBase % ss->W] < MAX_ATTEMPTS_OTHER) {
				if(ss->debug_mode())
					printf("WAIT_TIMEOUT: resending %d\n",ss->sndBase);
				
				if (ss->SendToUtil(ss->sndBase) != 1) {
					return FAILED_OTHER;
				}
				
				//todo check if mutex needed
				WaitForSingleObject(ss->params.mtx, INFINITE);
				ss->params.T += 1;
				ReleaseMutex(ss->params.mtx);

			}
			else {
				printf("MAX ATTEMPTS crossed for sndBase %d\n", ss->sndBase % ss->W);
				exit(0); // todo check if we should end here
				//return TIMEOUT;
			}

		}
		else if (signal == WAIT_OBJECT_0 + 2) {
			// worker_quit was signalled
			return 1;
		}else {
			//WAIT_FAILED
			//printf("Worker thread got WAIT_FAILED with error %d\n", GetLastError());
			
			/*
			* We can't stop the worker thread until we have received all the pending ACKS
			* for data packets sent.
			* A way to know this is by comparing the base with the next seq number. If they
			* are the same, we can say we are done receiving all required ACKs.
			*/
			if(ss->debug_mode())
				printf("sndBase:%d seq:%d\n", ss->sndBase, ss->seq);
			
			if(ss->sndBase == ss->seq){
				return 1;
			}
		}
	}

	return 0;
}

SenderSocket::SenderSocket()
{
	obj_st_time = hrc::now();        // get start time point
	is_conn_open = false;
	is_debug_mode = false;
	seq = 0;
	sndBase = 0;
	lastReleased = 0;

	params.obj_st_time = obj_st_time;
	
	
	// start the stats thread that shows statistics
	
	// Using std::thread library
	//statsThread = thread(showStats, &params);
	 
	// Create the events for synchronization 
	//Ref: https://docs.microsoft.com/en-us/windows/win32/sync

	params.mtx = CreateMutex(
		NULL,              // security attributes (default)
		FALSE,             // not owned initially
		NULL);             // unnamed

	params.event_quit = CreateEvent(
		NULL,	// security attributes (default)
		true,	// manual-reset event
		false,	// nonsignaled initial state
		NULL	// name
	);

	params.worker_quit = CreateEvent(
		NULL,	// security attributes (default)
		true,	// manual-reset event
		false,	// nonsignaled initial state
		NULL	// name
	);

	// TODO Check for null event creation and show error

	// using Win32 threads
	th_stats = CreateThread(
		NULL,								// security attributes (default)
		0,									// stack size (default) 
		(LPTHREAD_START_ROUTINE)showStats,	// thread function
		&params,							// parameters
		0,									// startup flags (default)
		NULL								// ID
	);

	if (th_stats == NULL)
	{
		printf("Stats thread CreateThread failed with error %d\n", GetLastError());
		return;
	}

}

int SenderSocket::Open(char* target_host, int rcv_port, int sender_window, LinkProperties* lp)
{
	// check if Close() hasn't been called after a call to Open()
	if (is_conn_open) {
		return ALREADY_CONNECTED;
	}

	W = sender_window;

	pkts = new SenderDataPkt[W]; // circular buffer of packets to be sent. 
	pkt_num_bytes = new int[W]; // stores number of bytes in packets at different indices
	//dup_acks = new int[W];
	dup_acks = 0;
	pkt_sent_time = new hrc::time_point[W];
	pkt_num_attempts = new int[W];

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
		if(debug_mode())
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
		if(debug_mode())
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
			if(debug_mode())
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
		
		if (debug_mode())
			printf("[ %.3f ] --> SYN %d (attempt %d of %d, RTO %.3f) to %s\n", elapsed, ss_hdr.sdh.seq, count, MAX_ATTEMPTS_SYN, rto, inet_ntoa(server.sin_addr));
		char* header_buf = (char*)&ss_hdr;
		// no connect needed, just send
		if (sendto(sock, header_buf, sizeof(ss_hdr), 0, (struct sockaddr*)&server, int(sizeof(server))) == SOCKET_ERROR) {
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			if (debug_mode())
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
				if (debug_mode())
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
			params.RTT = my_rto;

			if (debug_mode())
				printf("[ %.3f ] <-- SYN-ACK %d window %d; setting initial RTO to %.3f\n", elapsed, rcv_hdr->ackSeq, rcv_hdr->recvWnd, rto);

			// We mark the connection open on verifying the SYN-ACK received
			is_conn_open = true;


			/*
			* Create Semaphores and events for synchronization
			*/

			sock_recv_ready = WSACreateEvent();

			empty = CreateSemaphore(
				NULL,           // security attributes (default)
				0,				// initial count						//TODO check what should be the initial count. I think it should be W as all W slots are empty initially
				W,				// maximum count
				NULL);          // unnamed semaphore

			lastReleased = min(W, rcv_hdr->recvWnd);
			ReleaseSemaphore(empty, lastReleased, NULL);
			if (debug_mode())
				printf("ReleaseSemaphore empty by lastReleased = %d\n", lastReleased);

			full = CreateSemaphore(
				NULL,           // security attributes (default)
				0,				// initial count						// I think 0 because nothing is full initially
				W,				// maximum count
				NULL);          // unnamed semaphore
			
			th_worker = CreateThread(
				NULL,								// security attributes (default)
				0,									// stack size (default) 
				(LPTHREAD_START_ROUTINE)runWorker,	// thread function
				this,								// parameters
				0,									// startup flags (default)
				NULL								// ID
			);

			return STATUS_OK;

		}
		else if (ret < 0) {
			// ERROR
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			if (debug_mode())
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

int SenderSocket::Close(float *estimated_RTT)
{
	if (!is_conn_open) {
		return NOT_CONNECTED;
	}

	stopStats();

	hrc::time_point st;
	hrc::time_point fin_time;
	hrc::time_point cur_time;
	double elapsed;
	SenderSynHeader ss_hdr;
	ss_hdr.lp = my_lp;
	
	/*For the FIN phase, the RTO starts from the RTO derived during SYN phase.*/
	float rto = my_rto;

	ss_hdr.sdh.flags.FIN = 1;
	ss_hdr.sdh.seq = seq;

	int count = 1;
	while (count <= MAX_ATTEMPTS_OTHER) {
		//st = obj_st_time;
		fin_time = hrc::now();
		elapsed = ELAPSED(obj_st_time, fin_time);

		if (debug_mode())
			printf("[ %.3f ] --> FIN %d (attempt %d of %d, RTO %.3f) to %s\n", elapsed, ss_hdr.sdh.seq, count, MAX_ATTEMPTS_OTHER, rto, inet_ntoa(server.sin_addr));
		char* header_buf = (char*)&ss_hdr;
		// no connect needed, just send
		if (sendto(sock, header_buf, sizeof(ss_hdr), 0, (struct sockaddr*)&server, int(sizeof(server))) == SOCKET_ERROR) {
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			if (debug_mode())
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
				if (debug_mode())
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

			//if (debug_mode())
			//printf("[ %.3f ] <-- FIN-ACK %d window %d\n", elapsed, rcv_hdr->ackSeq, rcv_hdr->recvWnd);
			printf("[%.2f] <-- FIN-ACK %d %X\n", elapsed, rcv_hdr->ackSeq, rcv_hdr->recvWnd);
			
			// The connection can now be considered closed
			is_conn_open = false;

			*estimated_RTT = params.RTT;

			return STATUS_OK;

		}
		else if (ret < 0) {
			// ERROR
			cur_time = hrc::now();
			elapsed = ELAPSED(obj_st_time, cur_time);
			if (debug_mode())
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
	HANDLE arr[] = { params.event_quit, empty };
	WaitForMultipleObjects(2, arr, false, INFINITE);
	
	/*if (ret != WAIT_OBJECT_0) {
		printf("Wait for empty resulted in error %d\n", GetLastError());
		return FAILED_OTHER;
	}*/
	
	int slot = seq % W;
	
	hrc::time_point cur_time;
	double elapsed;
	hrc::time_point send_time;
	hrc::time_point send_ack_time;

	SenderDataPkt sender_pkt;
	sender_pkt.sdh.seq = seq;

	//sender_pkt.pktBuf = new char[numBytes];
	memcpy(sender_pkt.pktBuf, sendBuf, numBytes);

	pkts[slot] = sender_pkt;
	pkt_num_bytes[slot] = numBytes;
	pkt_sent_time[slot] = hrc::now();
	pkt_num_attempts[slot] = 0;

	if (debug_mode()) {
		printf("pkts[%d] now contains packet with seq %d\n", slot, seq);
		printf("inc seq from %d to %d\n", seq,seq+1);
	}

	seq++;
	
	ReleaseSemaphore(full, 1, NULL);

	return 0;
}

int SenderSocket::SendToUtil(int next_to_send)
{
	hrc::time_point cur_time;
	double elapsed;
	int idx = next_to_send % W;
	SenderDataPkt sender_pkt = pkts[idx];
	
	/*WaitForSingleObject(params.mtx, INFINITE);
	pkt_sent_time[idx] = hrc::now();
	ReleaseMutex(params.mtx);*/

	char* pkt_buf = (char*)&sender_pkt;
	int numBytes = pkt_num_bytes[idx];
	

	//if (debug_mode()) {
	//printf("--> next_to_send=%d and seq=%d\n", next_to_send, sender_pkt.sdh.seq);
	//}
	// todo remove this
	//std::this_thread::sleep_for(0.5ms);
	
	// no connect needed, just send
	if (sendto(sock, pkt_buf, sizeof(SenderDataHeader) + numBytes, 0, (struct sockaddr*)&server, int(sizeof(server))) == SOCKET_ERROR) {
		cur_time = hrc::now();
		elapsed = ELAPSED(obj_st_time, cur_time);
		if (debug_mode())
			printf("[ %.3f ] --> failed sendto with %d\n", elapsed, WSAGetLastError());
		//delete[] buf;
		return FAILED_SEND;
	}

	pkt_num_attempts[idx] += 1;

	return 1;
}

/// <summary>
/// move senderBase; fast retx 
/// </summary>
/// <returns></returns>
int SenderSocket::ReceiveACK()
{
	struct sockaddr_in response;
	int size = sizeof(response);
	char* recv_buf = new char[sizeof(ReceiverHeader)];
	int recv_res = recvfrom(sock, recv_buf, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size);

	hrc::time_point cur_time;
	double elapsed;
	hrc::time_point send_time;
	hrc::time_point send_ack_time;

	// error processing
	if (recv_res == SOCKET_ERROR) {
		cur_time = hrc::now();
		elapsed = ELAPSED(obj_st_time, cur_time);
		if (debug_mode())
			printf("[ %.3f ] <-- failed recvfrom with %d\n", elapsed, WSAGetLastError());
		//delete[] buf;
		//delete[] recvBuf;
		return FAILED_RECV;
	}

	ReceiverHeader* rcv_hdr = (ReceiverHeader*)recv_buf;

	send_ack_time = hrc::now();        // get end time point

	int y = rcv_hdr->ackSeq;
	if(debug_mode())
		printf("<-- ACK %d\n", y);

	//Todo check if this needs to be done
	/*WaitForSingleObject(params.mtx, INFINITE);
	params.nextSeq = y;
	ReleaseMutex(params.mtx);*/

	int oldBase = sndBase;

	/*
	*
	Third, upon receiving an ACK that moves the base from x to x + y, an RTT sample is
	computed only based on packet x + y – 1 and only if there were no prior retransmissions of base x

	TODO check how to factor in no prior retransmissions of base x
	*/
	// if base wasn't retransmitted
	if(y>sndBase && pkt_num_attempts[(y-1)%W]==1 && pkt_num_attempts[oldBase % W] == 1){
		send_time = pkt_sent_time[(y - 1) % W];
		elapsed = ELAPSED(send_time, send_ack_time); // in seconds
		double sampleRTT = elapsed;

		float oldRTT = params.RTT;
		//todo check if mutex needed
		WaitForSingleObject(params.mtx, INFINITE);
		params.RTT = ((1 - alpha) * params.RTT) + (alpha * sampleRTT);
		params.devRTT = ((1 - beta) * params.devRTT) + (beta * abs(sampleRTT - params.RTT));

		if (abs(params.RTT - oldRTT) > 1.0) {
			printf("oldRTT:%.3f newRTT:%.3f\n", oldRTT, params.RTT);
		}

		ReleaseMutex(params.mtx);

		

		// //RTO = estRTT + 4 * max (devRTT, 0.010);
		my_rto = params.RTT + 4 * max(params.devRTT, 0.010); //TODO should we set RTO here this way?
	}

	if (y > sndBase) {
		// successful ACK
		//if (debug_mode()) {
			//printf("<-- succ ACK\n");
		//}
		//printf("<-- succ ACK %d\n", y);
		int diff = sndBase - y;
		sndBase = y;
		dup_acks = 0;

		int effectiveWin = min(W, rcv_hdr->recvWnd);
		
		// how much we can advance the semaphore [ref: hw3p3.pdf]
		int newReleased = sndBase + effectiveWin - lastReleased;
		if (debug_mode())
			printf("<-- succ ACK %d newReleased (%d) = sndBase (%d) + effectiveWin (%d) - lastReleased (%d)\n", y, newReleased, sndBase, effectiveWin, lastReleased);

		ReleaseSemaphore(empty, newReleased, NULL);
		lastReleased += newReleased;

		//todo check if mutex needed
		WaitForSingleObject(params.mtx, INFINITE);
		params.MBacked += (MAX_PKT_SIZE * 1.0) / 1000000; //TODO check if this is correct way of doing it. We are finding no. of packets effectively acked and multiplying by MAX_PKT_SIZE
		params.base = sndBase;
		params.windowSize = effectiveWin;
		ReleaseMutex(params.mtx);
	}
	else {
		// duplicate ACK
		//if (debug_mode())
		
		dup_acks++;
		if(debug_mode())
			printf("<-- duplicate ACK %d | dup_acks=%d\n", y, dup_acks);

		if (dup_acks == FAST_RTX_DUP_THRESH) {
			if (debug_mode()) {
				printf("fast rtx for packet %d\n", y);
			}
			// We need to do fast retransmission for the base
			int res = SendToUtil(y);

			//todo check if mutex needed
			WaitForSingleObject(params.mtx, INFINITE);
			params.F++;
			ReleaseMutex(params.mtx);

		}
	}

	
	
	return STATUS_OK;

}

bool SenderSocket::is_all_sent() {
	// consumer has sent everything that the producer gave
	if (seq == params.nextSeq) {
		return true;
	}
	else {
		return false;
	}
}

void SenderSocket::stopStats() {
	/*statDone = true;
	cv.notify_all();
	this_thread::sleep_for(chrono::milliseconds(200));
	if (statsThread.joinable()) {
		statsThread.join();
	}*/

	// main.cpp (producer) is done populating packets in the shared buffer
	params.populating_complete = true;

	// But, the worker thread (consumer) may not have sent out all those packets
	// => Wait until worker thread has sent all packets
	
	//unique_lock<mutex> lck(mtx);
	//while (cv.wait_for(lck, 1s, is_all_sent) == false) {
	//	// do nothing
	//}
	while (seq != params.nextSeq) {
		std::this_thread::sleep_for(1000ms);
	}
	// Now, everything has been sent [but all ACKs may not have been obtained]

	if (debug_mode()){
		printf("stopstats called: seq: %d sndBase: %d params.nextSeq:%d\n", seq, sndBase, params.nextSeq);
		printf("setting event_quit...\n");
	}

	WaitForSingleObject(params.mtx, INFINITE);
	SetEvent(params.event_quit);
	ReleaseMutex(params.mtx);

	if (debug_mode())
		printf("event_quit has been set...");

	while (sndBase != seq) {
		std::this_thread::sleep_for(100ms);
	}

	WaitForSingleObject(params.mtx, INFINITE);
	SetEvent(params.worker_quit);
	ReleaseMutex(params.mtx);

	// wait for worker thread to end
	WaitForSingleObject(th_worker, INFINITE);
	CloseHandle(th_worker);

	if (debug_mode())
		printf("th_worker has been stopped...");

	// wait for stats to end
	WaitForSingleObject(th_stats, INFINITE);
	CloseHandle(th_stats);

	if (debug_mode())
		printf("th_stats has been stopped...");

}

