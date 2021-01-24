#include "pch.h"
#include "HttpResponseParser.h"

using namespace std;

HTTPresponse HttpResponseParser::parseHttpResponse(string recvBuf)
{
	HTTPresponse response;
	if (recvBuf.empty()) {
		return response;
	}

	// get the status code
	size_t found = recvBuf.find(" ");
	// status code is in the first line, after the first " " and is of length 3
	if ((found != string::npos) and (found + 3 < recvBuf.length())) {
		response.statusCode = atoi(recvBuf.substr(found + 1, 3).c_str());
	}

	// get the entire header (multiple lines)
	found = recvBuf.find("\r\n\r\n");
	if (found != string::npos) {
		response.header = recvBuf.substr(0, found);
	}

	// get the protocol (e.g., HTTP)
	found = recvBuf.find("/");
	if (found != string::npos) {
		response.protocol = recvBuf.substr(0, found);
	}

	// get the response object (html)
	found = recvBuf.find("\r\n\r\n");
	if (found != string::npos) {
		// skip over the four (not eight) characters
		response.object = recvBuf.substr(found + 4);
	}

	return response;
}

