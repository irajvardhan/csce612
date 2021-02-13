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
	numCode2xx = 0;
	numCode3xx = 0;
	numCode4xx = 0; 
	numCode5xx = 0; 
	numCodeOther = 0;

	duplicateHosts = 0;
	numRobotReqFail = 0;
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

void StatsManager::incrementNumCode2xx()
{
	numCode2xx += 1;
}

void StatsManager::incrementNumCode3xx()
{
	numCode3xx += 1;
}

void StatsManager::incrementNumCode4xx()
{
	numCode4xx += 1;
}

void StatsManager::incrementNumCode5xx()
{
	numCode5xx += 1;
}

void StatsManager::incrementNumCodeOther()
{
	numCodeOther += 1;
}

void StatsManager::incrementDuplicateHosts()
{
	duplicateHosts++;
}

void StatsManager::incrementRobotReqFail()
{
	numRobotReqFail++;
}
