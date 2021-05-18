#pragma once
/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
// SenderSocket.h
#include "pch.h"

class SenderSocket {
public:

	hrc::time_point obj_st_time;
	float my_rto;
	int seq;
	//Parameters params; //shared parameters

	std::thread statsThread;

	/* These will be accessed by both Open and Close*/
	SOCKET sock; // socket handle
	struct sockaddr_in server; // structure for connecting to server
	LinkProperties my_lp;
	SharedParameters params;

	bool is_conn_open;

	bool is_debug_mode;

	HANDLE th_stats, th_worker;
	HANDLE empty;
	HANDLE full;

	SenderDataPkt* pkts;
	int* pkt_num_bytes;
	int dup_acks;
	hrc::time_point* pkt_sent_time;
	int* pkt_num_attempts;

	int sndBase; // has to be synced with shared copy stored in SharedParameters
	int W; // window size
	int lastReleased;

	WSAEVENT sock_recv_ready;
	WSANETWORKEVENTS nw_events;

	// --done-- //

	SenderSocket();
	int Open(char* targetHost, int rcvPort, int senWindow, LinkProperties* lp);

	int Close(float* estimated_RTT);

	void stopThreadsUtil();

	int Send(char* sendBuf, int numBytes);

	int SendToUtil(int next_to_send);

	int ReceiveACK();

	bool debug_mode();

	bool is_all_sent();

};
