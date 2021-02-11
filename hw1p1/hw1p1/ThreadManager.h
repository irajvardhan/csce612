/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#pragma once
#include "pch.h"

class ThreadManager {
	

public:
	static std::queue<std::string> sharedQ;
	
	
	void initProducerConsumer(std::string content, int numThreads);

};