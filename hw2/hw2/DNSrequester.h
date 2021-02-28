#pragma once
#include "pch.h"
class DNSrequester {
public:
	// entry point for making a DNS request
	bool makeDNSrequest(std::string lookup, std::string dns_ip);

};