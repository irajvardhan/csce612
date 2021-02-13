/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#pragma once
#include "pch.h"

typedef struct url {
	std::string scheme;
	std::string host;
	int port;
	bool portSpecified;
	std::string path;
	std::string query;
	std::string fragment;
	bool isValid;
	std::string blameInvalidOn;
	int statusCode;
	int errorCode;

	url() {
		statusCode = -1;
		errorCode = 0;
	}

}URL;

class URLParser {

public:
	
	URL parseURL(std::string url);
	void displayURL(URL urlElems);
};