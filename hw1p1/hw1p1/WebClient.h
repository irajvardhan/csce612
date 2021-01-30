/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#pragma once
#include "pch.h"
#include "WebSocket.h"
#include "URLParser.h"

class WebClient {
	static std::unordered_set<DWORD> seenIPs;
	static std::unordered_set<std::string> seenHosts;

public:
	void crawl(std::string url);
	bool connectAndProcess(std::string type, struct sockaddr_in server, URL urlElems, 
		std::string request, std::string httpMethod, int minAllowedStatusCode, int maxAllowedStatusCode, int maxDownloadSize);
	static bool isIPunique(DWORD ip);
	static bool isHostUnique(std::string host);
	static void addIPtoSeen(DWORD ip);
	static void addHostToSeen(std::string host);
};