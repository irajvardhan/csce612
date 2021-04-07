/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#pragma once
#include "pch.h"

#define MAGIC_PROTOCOL 0x8311AA

#pragma pack(push,1)
class Flags {
public:
	DWORD reserved : 5; // must be zero
	DWORD SYN : 1;
	DWORD ACK : 1;
	DWORD FIN : 1;
	DWORD magic : 24;
	Flags() { memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL; }
};

class SenderDataHeader {
public:
	Flags flags;
	DWORD seq; // must begin from 0
};

class SenderSynHeader {
	public:
		SenderDataHeader sdh;
		LinkProperties lp;
};

class ReceiverHeader {
public:
	Flags flags;
	DWORD recvWnd; // receiver window for flow control (in pkts)
	DWORD ackSeq; // ack value = next expected sequence
};

class SenderDataPktHeader {
public:
	SenderDataHeader sdh;
	//char* pktBuf;
	char pktBuf[MAX_PKT_SIZE]; // TODO check if this should be higher
};

class SharedParameters {
public:
	int base;
	float MBacked;
	int nextSeq;
	int T;
	int F;
	int windowSize;
	float speed;
	float RTT;
	float devRTT;

	SharedParameters();

};

#pragma pack(pop)