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

/*
int HttpResponseParser::getStatusCode(string recvBuf)
{
	int statusCode = -1;
	if (recvBuf.empty()) {
		return statusCode;
	}

	size_t found = recvBuf.find(" ");
	// status code is in the first line, after the first " " and is of length 3
	if ((found != string::npos) and (found + 3 < recvBuf.length())) {
		statusCode = atoi(recvBuf.substr(found + 1, 3).c_str());
	}

	return statusCode;

}


string HttpResponseParser::getHTTPresponseHeader(string recvBuf) {
	string header = "";
	if (recvBuf.empty()) {
		return header;
	}

	size_t found = recvBuf.find("\r\n\r\n");
	if (found != string::npos) {
		header = recvBuf.substr(0, found);
	}

	return header;

}


string HttpResponseParser::getHTTPresponseObject(string recvBuf) {

	string responseObject = "";
	if (recvBuf.empty()) {
		return responseObject;
	}

	size_t found = recvBuf.find("\r\n\r\n");
	if (found != string::npos) {
		// skip over the four (not eight) characters
		responseObject = recvBuf.substr(found + 4);
	}

	return responseObject;

}

string HttpResponseParser::getHTTPresponseProtocol(string recvBuf) {

	string responseProtocol = "";
	if (recvBuf.empty()) {
		return responseProtocol;
	}

	size_t found = recvBuf.find("/");
	if (found != string::npos) {
		responseProtocol = recvBuf.substr(0, found);
	}

	return responseProtocol;
}
*/
