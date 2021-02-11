#include "pch.h"
#include "StatsManager.h"

StatsManager::StatsManager()
{
	e = 0;
	h = 0;
	d = 0;
	i = 0;
	r = 0;
	c = 0;
	l = 0;

	numRobotBytes = 0;
	numPageBytes = 0;
}

void StatsManager::incrementActiveThreads()
{
	q++;
}

void StatsManager::decrementActiveThreads()
{
	q--;
}

void StatsManager::incrementExtractedURLs()
{
	e++;
}

void StatsManager::incrementURLsWithUniqueHost()
{
	h++;
}

void StatsManager::incrementSuccessfulDNSLookups()
{
	d++;
}

void StatsManager::incrementURLsWithUniqueIP()
{
	i++;
}

void StatsManager::incrementURLsWhichPassedRobotCheck()
{
	r++;
}

void StatsManager::incrementURLsWithValidHTTPcode()
{
	c++;
}

void StatsManager::incrementLinksFound(int numLinks)
{
	l += numLinks;
}

void StatsManager::incrementNumRobotBytes(int bytes)
{
	numRobotBytes += bytes;
}

void StatsManager::incrementNumPageBytes(int bytes)
{
	numPageBytes += bytes;
}
