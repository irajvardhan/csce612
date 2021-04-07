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

	bool debug_mode;
	// --done-- //

	

	SenderSocket();
	int Open(char* targetHost, int rcvPort, int senWindow, LinkProperties* lp);

	int Close();

	// temp todo remove
	void stopStats();

	int Send(char* sendBuf, int numBytes);

};
