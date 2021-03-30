#pragma once
/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
// SenderSocket.h
#include "pch.h"
#define MAGIC_PORT 22345 // receiver listens on this port
#define MAX_PKT_SIZE (1500-28) // maximum UDP packet size accepted by receiver

class SenderSocket {
public:

	hrc::time_point obj_st_time;
	float my_rto;

	/* These will be accessed by both Open and Close*/
	SOCKET sock; // socket handle
	struct sockaddr_in server; // structure for connecting to server
	LinkProperties my_lp;

	bool is_conn_open;
	// --done-- //

	SenderSocket();
	int Open(char* targetHost, int rcvPort, int senWindow, LinkProperties* lp);

	int Close();

	int Send(char* sendBuf, int numBytes);

};
