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
}URL;

class URLParser {

public:
	URL parseURL(std::string url);
	void displayURL(URL urlElems);
};