/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
// hw3.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "SenderSocket.h"


using namespace std;

int main(int argc, char** argv)
{
	if (argc != 8) {
		printf("Incorrect arguments. \nUsage: %s <hostname/IP> <power-of-2-buffer-size> <sender-window-(in packets)> <RT propogation delay (in sec)> <prob loss FWD> <prob loss REV> <bottleneck speed (in Mbps)>\n", argv[0], "<url>");
		return 0;
	}
	
	hrc::time_point st;
	hrc::time_point en;
	double elapsed;

	// parse command-line parameters
	char* targetHost = argv[1];
	int power = atoi(argv[2]); 
	int senderWindow = atoi(argv[2]);
	UINT64 dwordBufSize = (UINT64) 1 << power; // UINT64 defined in Windows.h
	
	
	int status;
	LinkProperties lp;
	lp.RTT = atof(argv[4]);
	int link_speed_mbps = atoi(argv[7]);
	lp.speed = 1e6 * 1.0 * link_speed_mbps; // convert to bps
	lp.pLoss[FORWARD_PATH] = atof(argv[5]);
	lp.pLoss[RETURN_PATH] = atof(argv[6]);

	//Initialize WinSock; once per program run
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}
	
	printf("Main:\tsender W = %d, RTT %.3f sec, loss %g / %g, link %d Mbps\n", senderWindow, lp.RTT, lp.pLoss[FORWARD_PATH], lp.pLoss[RETURN_PATH], link_speed_mbps);
	
	printf("Main:\tintializing DWORD array with 2^%d elements...",power);
	st = hrc::now();        // get start time point
	DWORD* dwordBuf = new DWORD[dwordBufSize]; // user-requested buffer
	for (UINT64 i = 0; i < dwordBufSize; i++) // required initialization
		dwordBuf[i] = i;
	en = hrc::now();        // get end time point
	elapsed = ELAPSED_MS(st, en);
	printf("done in %d ms\n", int(elapsed));
	
	SenderSocket ss; // instance of your class
	if ((status = ss.Open(targetHost, MAGIC_PORT, senderWindow, &lp)) != STATUS_OK) {
		// error handling: print status and quit
		printf("Main:\t connect failed with status %d\n", status);
		delete dwordBuf;
		exit(0);
	}

	en = hrc::now();        // get end time point
	elapsed = ELAPSED(st, en);
	printf("Main:\t connected to %s in %.3f sec, pkt size %d bytes\n", targetHost, elapsed, MAX_PKT_SIZE);
	
	 
	
	char* charBuf = (char*)dwordBuf; // this buffer goes into socket
	
	UINT64 byteBufferSize = dwordBufSize << 2; // convert to bytes
	UINT64 off = 0; // current position in buffer
	while (off < byteBufferSize)
	{
		// decide the size of next chunk
		int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
		
		// send chunk into socket 
		if ((status = ss.Send(charBuf + off, bytes)) != STATUS_OK) {
			//error handing: print status and quit
			printf("Main:\Send failed with status %d", status);
			delete dwordBuf;
			exit(0);
		}
		off += bytes;
	}
	
	
	hrc::time_point before_transfer = hrc::now();
	if ((status = ss.Close()) != STATUS_OK) {
		// error handing: print status and quit
		printf("Main:\tClose failed with status %d", status);
		return 0;
	}
	hrc::time_point after_transfer = hrc::now();
	elapsed = ELAPSED(before_transfer, after_transfer);
	printf("Main:\t transfer finished in %.3f sec \n", elapsed);

	ss.stopStats();
	
	return 0;
}

