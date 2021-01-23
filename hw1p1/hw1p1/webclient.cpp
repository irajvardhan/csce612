#include "pch.h"
#include "WebClient.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

int getStatusCode(string recvBuf) 
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


string getHTTPresponseHeader(string recvBuf) {
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

string getHTTPresponseObject(string recvBuf) {

	string responseObject = "";
	if (recvBuf.empty()) {
		return responseObject;
	}

	size_t found = recvBuf.find("\r\n\r\n");
	if (found != string::npos) {
		responseObject = recvBuf.substr(found+8);
	}

	return responseObject;

}


void client(void) {
	// string pointing to an HTTP server (DNS name or IP)
	
	// variables for timing operations
	hrc::time_point st;
	hrc::time_point en;
	double elapsed;
	
	//char str[] = "https://www.tamu.edu/about/index.html#global-presence";
	//char str [] = "128.194.135.72";
	//string url = "http://balkans.aljazeera.net/misljenja";
	//string url = "http://tamu.edu";
	string url = "http://allybruener.com/";
	const char* str = url.c_str();

	printf("URL: %s\n", str);

	URLParser urlParser;
	URL urlElems = urlParser.parseURL(url);
	if (!urlElems.isValid) {
		printf("Invalid url\n");
		return;
	}
	string request = urlElems.path;
	if (!urlElems.query.empty()) {
		request = request + "?" + urlElems.query;
	}
	printf("host %s, port %d, request %s\n", urlElems.host.c_str(), urlElems.port, request.c_str());

	Socket webSocket;

	// open a TCP socket
	if (!webSocket.Open()) {
		printf("Error: Couldn't open socket");
		return;
	}

	// structure used in DNS lookups
	struct hostent* remote;

	// structure for connecting to server
	struct sockaddr_in server;

	printf("\t  Doing DNS... ");
	st = hrc::now();        // get start time point

	// first assume that the string is an IP address
	DWORD IP = inet_addr(urlElems.host.c_str());
	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(urlElems.host.c_str())) == NULL)
		{
			printf("Invalid string: neither FQDN, nor IP address\n");
			return;
		}
		else // take the first IP address and copy into sin_addr
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
	}
	else
	{
		// if a valid IP, directly drop its binary version into sin_addr
		server.sin_addr.S_un.S_addr = IP;
	}
	en = hrc::now();        // get end time point
	elapsed = ELAPSED_MS(st, en);
	printf("done in %.2f ms, found %s\n", elapsed, inet_ntoa(server.sin_addr));

	// setup the port # and protocol type
	server.sin_family = AF_INET;
	// host-to-network flips the byte order
	// TODO check if we need to find next open port for later assignment
	server.sin_port = htons(urlElems.port);
	
	// connect to the server on the specified or default port
	printf("\t* Connecting on page... ");
	st = hrc::now();        // get start time point
	if (!webSocket.Connect(server)) {
		printf("Error: failed to connect to server\n");
		// close the socket to this server; open again for the next one
		webSocket.Close();
		return;
	}
	en = hrc::now();        // get end time point
	elapsed = ELAPSED_MS(st, en);
	printf("done in %.2f ms\n", elapsed);

	//printf("Successfully connected to %s (%s) on port %d\n", str, inet_ntoa(server.sin_addr), ntohs(server.sin_port));

	// send HTTP requests here
	// string host = "www.google.com";
	//string httpRequest = "GET " + request + " HTTP/1.0\r\n" + "User-agent: rajcrawler/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n";
	string httpRequest = "GET " + request + " HTTP/1.0\r\n" + "Host: " + urlElems.host + "\r\n" + "User-agent: rajcrawler/1.1\r\n"  + "Connection: close\r\n\r\n";

	//cout << "Sending http request:\n" << httpRequest << endl;
	int requestLen = strlen(httpRequest.c_str());
	char* sendBuf = new char[requestLen + 1];
	strcpy_s(sendBuf, requestLen + 1, httpRequest.c_str());

	// send the request
	printf("\t  Loading... ");
	st = hrc::now();        // get start time point
	if (!webSocket.Send(sendBuf, requestLen)) {
		printf("Error: Send request failed.\n");
		// close the socket to this server; open again for the next one
		webSocket.Close();
		return;
	}

	// read from the socket
	if (!webSocket.Read()) {
		printf("Error: Failed to read from socket.\n");
		// close the socket to this server; open again for the next one
		webSocket.Close();
		return;
	}
	
	char* recvBuf = webSocket.GetBufferData();
	if (recvBuf == NULL) {
		printf("No data found in buffer\n");
		// close the socket to this server; open again for the next one
		webSocket.Close();
		return;
	}
	// close the socket to this server; open again for the next one
	webSocket.Close();

	en = hrc::now();        // get end time point
	elapsed = ELAPSED_MS(st, en);
	printf("done in %.2f ms with %zu bytes\n", elapsed, strlen(recvBuf));

	printf("\t  Verifying header... ");
	int statusCode = getStatusCode(string(recvBuf));
	printf("status code %d\n",statusCode);

	string responseHeader = getHTTPresponseHeader(string(recvBuf));

	// status code in 2xx format
	if (statusCode >= 200 && statusCode <= 299) {

		printf("\t  Parsing page... ");
		st = hrc::now();        // get start time point
		string responseObject = getHTTPresponseObject(string(recvBuf));
		//cout << "buffer data: " << recvBuf << endl;
		// create new parser object
		HTMLParserBase* parser = new HTMLParserBase;
		int nLinks;
		char* responseObjStr = new char[responseObject.length() + 1];
		strcpy_s(responseObjStr, requestLen + 1, responseObject.c_str());

		char* baseUrl = new char[urlElems.host.length() + 1];
		strcpy_s(baseUrl, urlElems.host.length() + 1, urlElems.host.c_str());

		char* linkBuffer = parser->Parse(responseObjStr, strlen(responseObjStr), baseUrl, (int)strlen(baseUrl), &nLinks);

		// check for errors indicated by negative values
		if (nLinks < 0) {
			nLinks = 0;
		}

		en = hrc::now();        // get end time point
		elapsed = ELAPSED_MS(st, en);
		printf("done in %.2f ms with %d links\n", elapsed, nLinks);
	}

	printf("----------------------------------------\n");
	printf("%s\n", responseHeader.c_str());
	

}