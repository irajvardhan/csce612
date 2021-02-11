/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#pragma once
#include "pch.h"

class StatsManager {


public:
	/*
	Q: current size of the pending queue
	E: number of extracted URLs from the queue
	H: number of URLs that have passed host uniqueness
	D: number of successful DNS lookups
	I: number of URLs that have passed IP uniqueness
	R: number of URLs that have passed robots checks
	C: number of successfully crawled URLs (those with a valid HTTP code)
	L: total links found
	*/
	std::atomic<int> q, e, h, d, i, r, c, l;
	
	
	StatsManager();

	void incrementActiveThreads();
	void decrementActiveThreads();
	void incrementExtractedURLs();
	void incrementURLsWithUniqueHost();
	void incrementSuccessfulDNSLookups();
	void incrementURLsWithUniqueIP();
	void incrementURLsWhichPassedRobotCheck();
	void incrementURLsWithValidHTTPcode();
	void incrementLinksFound(int numLinks);
	
};