// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include "cpu.h"
#include "DNS.h"
#include "WebClient.h"
#include "HTMLParserBase.h"
#include "HttpResponseParser.h"
#include <cstring>
#include <time.h>
#include <chrono>
#include <algorithm>


// using chrono high_resolution_clock
using namespace std::chrono;
using hrc = high_resolution_clock;
#define ELAPSED(st, en)			( duration_cast<duration<double>>(en - st).count() )
#define ELAPSED_MS(st, en)		( duration_cast<duration<double, std::milli>>(en - st).count() )

#endif //PCH_H
