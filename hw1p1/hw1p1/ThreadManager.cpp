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
bool productionOver = false;

void produce(string content) {
	istringstream contentStream(content);
	string line;
	while (getline(contentStream, line)) {
		if (line.empty())
			continue;
		//this_thread::sleep_for(chrono::seconds(3));
		// create a scope for this synchronized operation after which lock will be cleared
		{
			lock_guard<mutex> lck(mutex_q);
			ThreadManager::sharedQ.push(line);
		}
		
		cond_var.notify_one(); //TODO check if we have to do notify_one()/notify_all() here
	}

	// We are done pushing all URLs to queue. Now lets wakeup any waiting consumers so they can end themselves.
	{
		lock_guard<mutex> lck(mutex_q);
		productionOver = true;
	}
	cond_var.notify_all();
}

void consume() {

	// This consumer thread is now active
	statsManager.incrementActiveThreads();
	HTMLParserBase* parser = new HTMLParserBase;
	while (true) {
		// define a scope
		{
			unique_lock<mutex> lck(mutex_q);
			if (ThreadManager::sharedQ.empty() and productionOver) {
				break; // we are done
			}

			else if (ThreadManager::sharedQ.empty()) {
				continue;
				// TODO we can sleep for few seconds as well.
			}

			// Reaching here means we have something in the queue to consume.

			// wait until either there is something in the queue or production is completed
			cond_var.wait(lck, [] {return ThreadManager::sharedQ.size() > 0 || productionOver == true; });

			string line = ThreadManager::sharedQ.front();
			ThreadManager::sharedQ.pop();
			
			statsManager.incrementExtractedURLs();

			WebClient client;
			client.crawl(line, parser, statsManager);
		}
	}
	
	delete(parser);
	
	// This consumer thread is now inactive
	statsManager.decrementActiveThreads();
}

void showStats() {

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

	while (true) {

		// check if producer and consumers have stopped operation
		if (productionOver && (statsManager.q == 0)) {
			break;
		}

		this_thread::sleep_for(chrono::seconds(2));

		int pagesDownloadedTillNow = int(statsManager.c);
		int bytesDownloadedTillNow = int(statsManager.numRobotBytes) + int(statsManager.numPageBytes);
		en = hrc::now();        // get end time point
		elapsedSinceStart = (int)(ELAPSED_MS(st, en) / 1000); // in seconds

		elapsedSinceLastWakeup = ELAPSED_MS(prevTime, en) / 1000; // in seconds
		prevTime = en;

		printf("[%3d] %7d Q %6d E %7d H %6d D %6d I %5d R %5d C %5d L %4dK\n", elapsedSinceStart, int(ThreadManager::sharedQ.size()), int(statsManager.q), int(statsManager.e), int(statsManager.h), int(statsManager.d), int(statsManager.i), int(statsManager.r), int(statsManager.c), int(statsManager.l/1000));
		
		int pagesDownloadedSinceLastWakeup = pagesDownloadedTillNow - numPagesDownloadedPrev;
		numPagesDownloadedPrev = pagesDownloadedTillNow;
		double crawlSpeedSinceLastWakeup = pagesDownloadedSinceLastWakeup / elapsedSinceLastWakeup;

		int bytesDownloadedSinceLastWakeup = bytesDownloadedTillNow - numBytesDownloadedPrev;
		numBytesDownloadedPrev = bytesDownloadedTillNow;
		double bandwidthSinceLastWakeup = (numBytesDownloadedPrev * 8) / (1000000 * elapsedSinceLastWakeup);
		printf("*** crawling %.1f pps @ %.1f Mbps\n", crawlSpeedSinceLastWakeup, bandwidthSinceLastWakeup);
	}
}

void ThreadManager::initProducerConsumer(string content, int numThreads)
{
	// producer thread that reads URLs and feeds to a shared queue
	thread producerThread(produce, content);

	// start the stats thread that shows statistics
	thread statsManagerThread(showStats);

	// start consumer threads that will read URLs from shared queue and crawl them
	thread* consumerThreads = new thread[numThreads];
	int i = 0;
	while (i < numThreads) {
		consumerThreads[i] = thread(consume);
		++i;
	}

	if (producerThread.joinable()) {
		producerThread.join();
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
