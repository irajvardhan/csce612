/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/

#include "pch.h"
#include "HttpResponseParser.h"

using namespace std;

HTTPresponse HttpResponseParser::parseHttpResponse(string recvBuf)
{
	HTTPresponse response;
	
	try {
		if (recvBuf.empty()) {
			throw "Error: received empty buffer";
		}

		// get the status code
		size_t found = recvBuf.find(" ");
		// status code is in the first line, after the first " " and is of length 3
		if ((found != string::npos) and (found + 3 < recvBuf.length())) {
			response.statusCode = atoi(recvBuf.substr(found + 1, 3).c_str());
		}
		else {
			throw "Error: unable to find status code";
		}

		// get the entire header (multiple lines)
		found = recvBuf.find("\r\n\r\n");
		if (found != string::npos) {
			response.header = recvBuf.substr(0, found);
		}
		else {
			throw "Error: unable to find header";
		}

		// get the protocol (e.g., HTTP)
		found = recvBuf.find("/");
		if (found != string::npos) {
			response.protocol = recvBuf.substr(0, found);
			if (response.protocol.compare("HTTP") != 0) {
				throw "Error: response not in HTTP protocol";
			}
		}
		else {
			throw "Error: unable to find protocol";
		}

		// get the response object (html)
		found = recvBuf.find("\r\n\r\n");
		if (found != string::npos) {
			// skip over the four (not eight) characters
			response.object = recvBuf.substr(found + 4);
		}
		else {
			throw "Error: unable to find object";
		}

	}
	catch (const char* msg) {
		//printf("Error: %s", msg);
		return response;
	}

	response.isValid = true;

	return response;
}

