// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here

#endif //PCH_H

#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)
#define MAX_DNS_LEN 512

#define LOOKUP_IS_HOST 0
#define LOOKUP_IS_IP 1

#define MAX_ATTEMPTS 3
#define MAX_DNS_SIZE 512 // largest valid UDP packet

/* DNS query types */
#define DNS_NS 2 /* name server */
#define DNS_MX 15 /* mail exchange */
#define DNS_AXFR 252 /* request for zone transfer */
#define DNS_CNAME 5 /* canonical name */
#define DNS_A 1 /* name -> IP */
#define DNS_PTR 12 /* IP -> name */
#define DNS_HINFO 13 /* host info/SOA */
#define DNS_ANY 255 /* all records */

/* query classes */
#define DNS_INET 1

/* flags */
#define DNS_QUERY (0 << 15) /* 0 = query; 1 = response */
#define DNS_RESPONSE (1 << 15)
#define DNS_STDQUERY (0 << 11) /* opcode - 4 bits */
#define DNS_AA (1 << 10) /* authoritative answer */
#define DNS_TC (1 << 9) /* truncated */
#define DNS_RD (1 << 8) /* recursion desired */
#define DNS_RA (1 << 7) /* recursion available */

/* common result codes */
#define DNS_OK 0 /* success */
#define DNS_FORMAT 1 /* format error (unable to interpret) */
#define DNS_SERVERFAIL 2 /* can’t find authority nameserver */
#define DNS_ERROR 3 /* no DNS entry */
#define DNS_NOTIMPL 4 /* not implemented */
#define DNS_REFUSED 5 /* server refused the query */

// add headers that you want to pre-compile here
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <cstring>
#include <time.h>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_set>
#include "WebSocket.h"


typedef unsigned short int USHORT;

#include "DNSheader.h"
#include "DNSrequester.h"

// using chrono high_resolution_clock
using namespace std::chrono;
using hrc = high_resolution_clock;
#define ELAPSED(st, en)			( duration_cast<duration<double>>(en - st).count() )
#define ELAPSED_MS(st, en)		( duration_cast<duration<double, std::milli>>(en - st).count() )