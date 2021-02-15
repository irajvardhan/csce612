/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "ThreadManager.h"
#pragma comment(lib, "ws2_32.lib")

using namespace std;

queue<string> ThreadManager::sharedQ{};
condition_variable cond_var;
StatsManager statsManager;

/*Mutex Declarations*/
mutex mutex_q;		// mutex on the shared queue

void consume() {

	// This consumer thread is now active
	statsManager.incrementActiveThreads();
	HTMLParserBase* parser = new HTMLParserBase;
	while (true) {
		string line;
		// define a scope
		{
			lock_guard<mutex> lck(mutex_q);
			
			if (ThreadManager::sharedQ.empty()) {
				break; // we are done
			}

			line = ThreadManager::sharedQ.front();
			ThreadManager::sharedQ.pop();
		}
		
		statsManager.incrementExtractedURLs();
		//printf()
		WebClient client;
		client.crawl(line, parser, statsManager);
	}
	
	delete(parser);
	
	// This consumer thread is now inactive
	statsManager.decrementActiveThreads();
}

void showStats() {
	//return; // remove this
	hrc::time_point st;
	hrc::time_point en;
	hrc::time_point prevTime;

	// how much time has elapsed (in seconds) since the last wakeup
	double elapsedSinceLastWakeup;

	// how much time has elapsed (in seconds) since the start 
	int elapsedSinceStart;

	st = hrc::now();        // get start time point
	prevTime = st;

	int numPagesDownloadedPrev = 0;
	int numBytesDownloadedPrev = 0;
	bool allOver = false;

	while (true) {

		this_thread::sleep_for(chrono::seconds(2));

		// check if queue has become empty
		// TODO put an event here
		if (ThreadManager::sharedQ.size()==0) {
			if (statsManager.q == 0 && allOver) {
				break;
			}
			else if(statsManager.q == 0) {
				// this essentially prints the local stats one final time.
				allOver = true;
			}
		}

		int pagesDownloadedTillNow = int(statsManager.c);
		int bytesDownloadedTillNow = int(statsManager.numRobotBytes) + int(statsManager.numPageBytes);
		en = hrc::now();        // get end time point
		elapsedSinceStart = (int)(ELAPSED_MS(st, en) / 1000); // in seconds

		elapsedSinceLastWakeup = ELAPSED_MS(prevTime, en) / 1000; // in seconds
		prevTime = en;

		printf("[%3d] %7d Q %6d E %7d H %6d D %6d I %5d R %5d C %5d L %4dK\n", elapsedSinceStart, int(statsManager.q), int(ThreadManager::sharedQ.size()), int(statsManager.e), int(statsManager.h), int(statsManager.d), int(statsManager.i), int(statsManager.r), int(statsManager.c), int(statsManager.l/1000));
		
		int pagesDownloadedSinceLastWakeup = pagesDownloadedTillNow - numPagesDownloadedPrev;
		numPagesDownloadedPrev = pagesDownloadedTillNow;
		double crawlSpeedSinceLastWakeup = pagesDownloadedSinceLastWakeup / elapsedSinceLastWakeup;

		int bytesDownloadedSinceLastWakeup = bytesDownloadedTillNow - numBytesDownloadedPrev;
		numBytesDownloadedPrev = bytesDownloadedTillNow;
		double bandwidthSinceLastWakeup = double((bytesDownloadedSinceLastWakeup * 8) / (1000000 * elapsedSinceLastWakeup));
		printf("*** crawling %.1f pps @ %.1f Mbps\n", crawlSpeedSinceLastWakeup, bandwidthSinceLastWakeup);
		
	}
	en = hrc::now();        // get end time point
	elapsedSinceStart = (int)(ELAPSED_MS(st, en) / 1000); // in seconds
	double rate = statsManager.e / elapsedSinceStart;
	printf("Extracted %d URLs @ %.2f/s\n", int(statsManager.e), rate);
	
	rate = int(statsManager.h) / elapsedSinceStart;
	printf("Looked up %d DNS names @ %.2f/s\n", int(statsManager.h), rate);
	
	// remove this
	//printf("Duplicate hosts: %d\n", int(statsManager.duplicateHosts));
	//printf("Number of failed Robot requests: %d\n", int(statsManager.numRobotReqFail));
	
	rate = int(statsManager.i) / elapsedSinceStart;
	printf("Downloaded %d robots @ %.2f/s\n", int(statsManager.i), rate);

	rate = int(statsManager.c) / elapsedSinceStart;
	double downloadedMB = statsManager.numPageBytes/(1024*1024);
	printf("Crawled %d pages @ %.2f/s (%.2fMB)\n", int(statsManager.c), rate, downloadedMB);
	
	rate = int(statsManager.l) / elapsedSinceStart;
	printf("Parsed %d links @%.2f/s\n", int(statsManager.l), rate);

	printf("HTTP codes: 2xx = %d, 3xx = %d, 4xx = %d, 5xx = %d, other = %d\n", int(statsManager.numCode2xx), int(statsManager.numCode3xx), int(statsManager.numCode4xx), int(statsManager.numCode5xx), int(statsManager.numCodeOther));

	printf("%d links point to a TAMU website. Out of these, %d originate from outside of TAMU\n", int(statsManager.numTAMUlinks), int(statsManager.numLinksFromOutsideTAMU));
	printf("However, overall there were %d links which contained tamu.edu anywhere in them.\n", int(statsManager.numLinksContainingTAMUAnywhere));
	printf("%d pages contain a link that points to a TAMU website. Out of these, %d originate from outside of TAMU\n", int(statsManager.numPagesContainingTamuLink), int(statsManager.numPagesFromOutsideTamu));

}

void ThreadManager::initProducerConsumer(string content, int numThreads)
{
	// producer [main thread] reads URLs and feeds to a shared queue
	istringstream contentStream(content);
	string line;
	while (getline(contentStream, line)) {
		if (line.empty())
			continue;
		stringstream ss(line);
		std::string trimmed_line;
		ss >> trimmed_line;
		ThreadManager::sharedQ.push(trimmed_line);
	}

	// start the stats thread that shows statistics
	thread statsManagerThread(showStats);

	// start consumer threads that will read URLs from shared queue and crawl them
	thread* consumerThreads = new thread[numThreads];
	int i = 0;
	while (i < numThreads) {
		consumerThreads[i] = thread(consume);
		++i;
	}

	i = 0;
	while (i < numThreads) {
		if (consumerThreads[i].joinable()) {
			consumerThreads[i].join();
		}
		++i;
	}

	if (statsManagerThread.joinable()) {
		statsManagerThread.join();
	}

}
