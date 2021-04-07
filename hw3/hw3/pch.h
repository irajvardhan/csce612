/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// The full list of errors that SocketSender functions should be able to produce is given by
#define STATUS_OK 0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED 2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME 3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND 4 // sendto() failed in kernel
#define TIMEOUT 5 // timeout after all retx attempts are exhausted
#define FAILED_RECV 6 // recvfrom() failed in kernel
#define FAILED_OTHER -1 // genereic failures
#define INVALID_SOCKET -2 // invalid socket error
#define BIND_SOCKET_FAILED -3 // bind socket failed

#define MAX_ATTEMPTS_SYN 3 // for SYN packets
#define MAX_ATTEMPTS_OTHER 5 // for all other except SYN

#define MAGIC_PORT 22345 // receiver listens on this port
#define MAX_PKT_SIZE (1500-28) // maximum UDP packet size accepted by receiver


// using chrono high_resolution_clock
#include <time.h>
#include <chrono>
using namespace std::chrono;
using hrc = high_resolution_clock;
#define ELAPSED(st, en)			( duration_cast<duration<double>>(en - st).count() )
#define ELAPSED_MS(st, en)		( duration_cast<duration<double, std::milli>>(en - st).count() )


// add headers that you want to pre-compile here
#include <iostream>
#include <Windows.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <math.h>
#include "LinkProperties.h"
#include "PacketHeaders.h"
#include "SenderSocket.h"




#endif //PCH_H
