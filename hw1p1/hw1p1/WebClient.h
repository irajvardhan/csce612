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
	void crawl(std::string url); //used only for part 1
	void crawl(std::string url, HTMLParserBase* parser, StatsManager& statsManager); // used for part 3
	bool connectAndProcess(std::string type, struct sockaddr_in server, URL& urlElems, 
		std::string request, std::string httpMethod, int minAllowedStatusCode, int maxAllowedStatusCode, int maxDownloadSize, HTMLParserBase* parser, StatsManager& statsManager);
	static std::pair<std::unordered_set<DWORD>::iterator, bool> addIPtoSeen(DWORD ip);
	static std::pair<std::unordered_set<std::string>::iterator, bool> addHostToSeen(std::string host);
};