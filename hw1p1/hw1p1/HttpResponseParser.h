#pragma once
#include "pch.h"

typedef struct httpResponse {
	int statusCode;
	std::string header;
	std::string object;
	std::string protocol;
	std::string protocolVersion;

	httpResponse() {
		statusCode = -1;
		header = "";
		object = "";
		protocol = "";
		protocolVersion = "";
	}
}HTTPresponse;

class HttpResponseParser {
public:
	HTTPresponse parseHttpResponse(std::string recvBuf);
};