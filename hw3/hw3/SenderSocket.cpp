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
float ALPHA = 0.125; // used for RTO computation
float BETA = 0.25;

/// <summary>
/// Helps control the frequency of debug outputs.
/// This is particularly useful in multithreaded environment
/// when you may not want thousands of lines being printed.
/// </summary>
/// <returns>boolean output signifying whether caller should print or not</returns>
bool SenderSocket::debug_mode() {
	
	// Directly return false if you dont want ANY debug print
	// or		return true  if you      want ALL debug print
	// return false;
	
	// The higher this value, the less will be the likely of 
	// this function returning true
	int upper = 100;
	int r = rand() % upper;
	
	// return true with probability of 1/upper
	if (r == 0)
		return true;

	return false;
}

/// <summary>
/// Function to show statistics at regular time intervals
/// </summary>
/// <param name="lpParams"></param>
/// <returns></returns>
DWORD WINAPI showStats(LPVOID lpParams) {

	SharedParameters* params = (SharedParameters*)lpParams;
	hrc::time_point cur_time;
	double elapsed;
	double elapsed_interval;
	unique_lock<mutex> lck(mtx);

	int prev_base = 0;
	hrc::time_point prev_time = params->obj_st_time;
	int count_of_same = 0;

	// The while loop code will be executed every 2 seconds
	// and will break if event_quit is set
	while (WaitForSingleObject(params->event_quit, 2000) == WAIT_TIMEOUT) {
		cur_time = hrc::now();
		elapsed = ELAPSED(params->obj_st_time, cur_time);
		
		// determine the total Mbits received in this interval (since stats were last printed)
		float Mbits_received_interval = (params->base - prev_base) * ((MAX_PKT_SIZE - sizeof(SenderDataHeader))/1e6) * 8;

		prev_base = params->base;
		elapsed_interval = ELAPSED(prev_time, cur_time);
		prev_time = cur_time;
		
		// Modification of sharaed params can be wrapped with a mutex
		// although it is not necessary in current implementation
		WaitForSingleObject(params->mtx, INFINITE);
		params->speed = Mbits_received_interval / elapsed_interval; // in Mbps	
		ReleaseMutex(params->mtx);
		
		// print the stats
		printf("[%4d] B %8d ( %5.1f MB) N %8d T %4d F%4d W %4d S %4.3f Mbps RTT %.3f\n", int(elapsed), params->base, params->MBacked, params->nextSeq,
			params->T, params->F, params->windowSize, params->speed,
			params->RTT
		);
	}
	
	return 0;

}

// Worker thread
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

	HANDLE arr_handles[] = { ss->sock_recv_ready, ss->full, ss->params.worker_quit }; 

	DWORD timeout;

	// This will be incremented by worked thread from here onwards
	int next_to_send = ss->params.base;

	while (true) {
		// TODO find out what should be the condition here to check for pending packets
		if (ss->params.nextSeq >= 1) {
			timeout = 1000*ss->my_rto; //my_rto is in sec so we need to convert to ms as expecte by WaitForMultipleObjects
			ss->params.temp_rto = timeout;
		}
		else {
			timeout = INFINITE;
		}
		
		//int signal = WaitForMultipleObjects(2, arr_handles, false, timeout);
		int signal = WaitForMultipleObjects(3, arr_handles, false, timeout);

		if (signal == WAIT_OBJECT_0) {
			// Ref: https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumnetworkevents
			//WSAEnumNetworkEvents(ss->sock, ss->sock_recv_ready, &ss->nw_events); // Without this line ss->ReceiveACK() keeps getting called
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
			//  check if we are done sending everything
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
				printf("MAX ATTEMPTS crossed for sndBase %d\n", ss->sndBase);
				exit(0); // todo check if we should end here
				//return TIMEOUT;
			}

		}
		else if (signal == WAIT_OBJECT_0 + 2) {
			// worker_quit was signalled
			return 1;
		}else {
			//WAIT_FAILED
			
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

// Constructor
SenderSocket::SenderSocket()
{
	obj_st_time = hrc::now();        // get start time point
	is_conn_open = false;
	is_debug_mode = false;
	seq = 0;
	sndBase = 0;
	lastReleased = 0;

	params.obj_st_time = obj_st_time;
	 
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

	// TODO We can add check for null event creation and show error

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

// Open a connection 
// [includes SYN ACK phase]
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
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		cur_time = hrc::now();
		elapsed = ELAPSED(obj_st_time, cur_time);
		if(debug_mode())
			printf("[ %.3f ] --> socket open failed with %d\n", elapsed, WSAGetLastError());
		
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
			* If we dont get back a SYN ACK then 
			* it means something went wrong. Try again.
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

			//sock_recv_ready = WSACreateEvent();
			sock_recv_ready = CreateEvent(
				NULL,	// security attributes (default)
				false,	// manual-reset event
				false,	// nonsignaled initial state
				NULL	// name
			);

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

// Close a connection 
// [includes waiting for threads to end and then FIN ACK phase]
int SenderSocket::Close(float *estimated_RTT)
{
	if (!is_conn_open) {
		return NOT_CONNECTED;
	}

	stopThreadsUtil();

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

// Send a packet
int SenderSocket::Send(char* sendBuf, int numBytes)
{
	HANDLE arr[] = { params.event_quit, empty };
	WaitForMultipleObjects(2, arr, false, INFINITE);
	
	int slot = seq % W;
	
	hrc::time_point cur_time;
	double elapsed;
	hrc::time_point send_time;
	hrc::time_point send_ack_time;

	SenderDataPkt sender_pkt;
	sender_pkt.sdh.seq = seq;

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

	char* pkt_buf = (char*)&sender_pkt;
	int numBytes = pkt_num_bytes[idx];
	
	if (debug_mode()) {
		printf("--> next_to_send=%d and seq=%d\n", next_to_send, sender_pkt.sdh.seq);
	}
	
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

	int oldBase = sndBase;

	/*
	*
	Third, upon receiving an ACK that moves the base from x to x + y, an RTT sample is
	computed only based on packet x + y – 1 and only if there were no prior retransmissions of base x
	*/
	// if base wasn't retransmitted
	if(y>sndBase && pkt_num_attempts[(y-1)%W]==1 && pkt_num_attempts[oldBase % W] == 1){
		send_time = pkt_sent_time[(y - 1) % W];
		elapsed = ELAPSED(send_time, send_ack_time); // in seconds
		double sampleRTT = elapsed;
		WaitForSingleObject(params.mtx, INFINITE);
		params.RTT = ((1 - ALPHA) * params.RTT) + (ALPHA * sampleRTT);
		params.devRTT = ((1 - BETA) * params.devRTT) + (BETA * abs(sampleRTT - params.RTT));
		ReleaseMutex(params.mtx);

		my_rto = params.RTT + 4 * max(params.devRTT, 0.010); 
	}

	if (y > sndBase) {
		// successful ACK
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

// Checks if all packets have been sent
bool SenderSocket::is_all_sent() {
	// consumer has sent everything that the producer gave
	if (seq == params.nextSeq)
		return true;
	
	return false;
	
}

// Safely stops all threads and waits wherever required
void SenderSocket::stopThreadsUtil() {
	// main.cpp (producer) is done populating packets in the shared buffer
	params.populating_complete = true;

	// Worker thread (consumer) may not have sent out all those packets
	// Wait until worker thread has sent all packets
	while (seq != params.nextSeq) {
		std::this_thread::sleep_for(1000ms);
	}
	
	if (debug_mode()){
		printf("stopstats called: seq: %d sndBase: %d params.nextSeq:%d\n", seq, sndBase, params.nextSeq);
		printf("setting event_quit...\n");
	}

	WaitForSingleObject(params.mtx, INFINITE);
	SetEvent(params.event_quit);
	ReleaseMutex(params.mtx);

	// All packets have been sent
	// wait until all ACKs have been obtained
	while (sndBase != seq) {
		std::this_thread::sleep_for(100ms);
	}

	// Now we can stop the worker and stats threads
	WaitForSingleObject(params.mtx, INFINITE);
	SetEvent(params.worker_quit);
	ReleaseMutex(params.mtx);

	// wait for worker thread to end
	WaitForSingleObject(th_worker, INFINITE);
	CloseHandle(th_worker);

	// wait for stats to end
	WaitForSingleObject(th_stats, INFINITE);
	CloseHandle(th_stats);

}

