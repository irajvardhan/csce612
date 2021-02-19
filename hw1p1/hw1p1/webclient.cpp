/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "WebClient.h"
#pragma comment(lib, "ws2_32.lib")

using namespace std;

mutex mutex_host;
mutex mutex_ip;
mutex mutex_print;

std::unordered_set<DWORD> WebClient::seenIPs{};
std::unordered_set<std::string> WebClient::seenHosts{};


pair<unordered_set<DWORD>::iterator, bool> WebClient::addIPtoSeen(DWORD ip)
{
	{
		lock_guard<mutex> lck(mutex_ip);
		return seenIPs.insert(ip);
	}
}

pair<unordered_set<string>::iterator, bool>  WebClient::addHostToSeen(string host)
{
	{
		lock_guard<mutex> lck(mutex_host);
		return seenHosts.insert(host);
	}
	
}

bool isHostTamu(string url) 
{
	if (url.empty()) {
		return false;
	}
	
	if (url.compare("tamu.edu") == 0) {
		return true;
	}

	// correct = abc.tamu.edu , x.tamu.edu
	// wrong = .tamu.edu, abctamu.edu, tamu.edu89
	if (url.size() > 9) {
		int l = url.size();
		int st = l - 9;
		if (url.substr(st).compare(".tamu.edu") == 0) {
			return true;
		}
	}

	return false;
}

/*
* Dechunks HTTP 1.1 responses
* input: body consisting of many chunks
* returns: new body with chunks one after the other (their sizes are removed from between)
*/
string dechunk(string body) 
{
	string rem_body = body;
	string newBody = "";
	string chunkSizeStr = "";
	unsigned int chunk_len = 0;
	try {
		while (true) {
			if (chunk_len == 0) {
				size_t found = rem_body.find("\r\n");
				if (found != string::npos) {
					chunkSizeStr = rem_body.substr(0, found);
					rem_body = rem_body.substr(found + 2);
					stringstream ss;
					ss << hex << chunkSizeStr;
					ss >> chunk_len;

					// No more chunks after this
					if (chunk_len == 0) {
						break;
					}
				}
			}
			else {
				string part = rem_body.substr(0, chunk_len);
				rem_body = rem_body.substr(chunk_len + 2);
				newBody += part;
				chunk_len = 0;
			}
		}
	}
	catch (...) {
		printf("failed\n");
		return body;
	}

	// dechunking successful
	
	// null terminate
	newBody += '\0';
	
	return newBody;
}



// to be used for part 1 for HTTP/1.1
void WebClient::crawl(std::string url)
{
	// string pointing to an HTTP server (DNS name or IP)
	// variables for timing operations
	hrc::time_point st;
	hrc::time_point en;
	double elapsed;

	//char str[] = "https://www.tamu.edu/about/index.html#global-presence";
	//char str [] = "128.194.135.72";
	//string url = "http://balkans.aljazeera.net/misljenja";
	//string url = "http://tamu.edu";
	//string url = "http://allybruener.com/"; // should give 2xx 
	//string url = "https://google.com"; // failed with invalid scheme
	//string url = "http://xyz.com:0/"; // failed with invalid port
	//string url = "http://goopoiopoipoiopxx.com"; // 
	const char* str = url.c_str();

	printf("URL: %s\n", str);

	URLParser urlParser;
	URL urlElems = urlParser.parseURL(url);
	if (!urlElems.isValid) {
		printf("failed with invalid %s\n", urlElems.blameInvalidOn.c_str());
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
		printf("failed as couldn't open socket\n");
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
			printf("failed with %d\n", WSAGetLastError());
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
		printf("failed with %d\n", WSAGetLastError());
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
	
	// 1.0 request
	//string httpRequest = "GET " + request + " HTTP/1.0\r\n" + "Host: " + urlElems.host + "\r\n" + "User-agent: rajcrawler/1.1\r\n" + "Connection: close\r\n\r\n";

	// 1.1 request
	string httpRequest = "GET " + request + " HTTP/1.1\r\n" + "Host: " + urlElems.host + "\r\n" + "User-agent: rajcrawler/1.1\r\n" + "Connection: close\r\n\r\n";


	//cout << "Sending http request:\n" << httpRequest << endl;
	int requestLen = strlen(httpRequest.c_str());
	char* sendBuf = new char[requestLen + 1];
	strcpy_s(sendBuf, requestLen + 1, httpRequest.c_str());

	// send the request
	printf("\t  Loading... ");
	st = hrc::now();        // get start time point
	if (!webSocket.Send(sendBuf, requestLen)) {
		printf("failed with %d\n", WSAGetLastError());
		// close the socket to this server; open again for the next one
		webSocket.Close();
		return;
	}

	// read from the socket
	int maxDownloadSize = MB(200); // set it high as its not needed in part1
	RecvOutcome outcome = webSocket.Read(maxDownloadSize);
	if (!outcome.success) {
		if (!outcome.errorAlreadyShown) {
			printf("failed with %d on recv\n", WSAGetLastError());
		}
		webSocket.Close();
		return;
	}

	char* recvBuf = webSocket.GetBufferData();
	if (recvBuf == NULL) {
		printf("failed as no data found in buffer\n");
		// close the socket to this server; open again for the next one
		webSocket.Close();
		return;
	}
	// close the socket to this server; open again for the next one
	webSocket.Close();

	HttpResponseParser httpReponseParser;
	HTTPresponse response = httpReponseParser.parseHttpResponse(recvBuf);

	// check for invalid response
	if (!response.isValid) {
		printf("failed with non-HTTP header\n");
		return;
	}

	en = hrc::now();        // get end time point
	elapsed = ELAPSED_MS(st, en);
	printf("done in %.2f ms with %zu bytes\n", elapsed, strlen(recvBuf));

	printf("\t  Verifying header... ");
	printf("status code %d\n", response.statusCode);

	// status code in 2xx format
	if (response.statusCode >= 200 && response.statusCode <= 299) {
		int bodyLen = response.object.length();
		if (response.isChunked) {
			printf("\t  Dechunking...body size was %d, ", bodyLen); //minus 1 is just to avoid size of null termination
			string bodyDechunked = dechunk(response.object);
			printf("now %d\n", int(bodyDechunked.size()-1));
			response.object = bodyDechunked;
		}
		else {
			//printf("\t  Response is not chunked...body size is %d", bodyLen);
		}
		

		printf("\t+ Parsing page... ");
		st = hrc::now();        // get start time point		
		// create new parser object
		HTMLParserBase* parser = new HTMLParserBase;
		int nLinks;
		char* responseObjStr = new char[response.object.length() + 1];
		strcpy_s(responseObjStr, response.object.length() + 1, response.object.c_str());

		string baseUrl;
		if (urlElems.host.substr(0, 4).compare("www.") == 0) {
			baseUrl = "http://" + urlElems.host;
		}
		else {
			baseUrl = "http://www." + urlElems.host;
		}

		char* baseUrlstr = new char[baseUrl.length() + 1];
		strcpy_s(baseUrlstr, baseUrl.length() + 1, baseUrl.c_str());

		char* linkBuffer = parser->Parse(responseObjStr, strlen(responseObjStr), baseUrlstr, (int)strlen(baseUrlstr), &nLinks);

		// check for errors indicated by negative values
		if (nLinks < 0) {
			nLinks = 0;
		}

		en = hrc::now();        // get end time point
		elapsed = ELAPSED_MS(st, en);
		printf("done in %.2f ms with %d links\n\n", elapsed, nLinks);
	}

	printf("----------------------------------------\n");
	printf("%s\n", response.header.c_str());

}

void WebClient::crawl(string url, HTMLParserBase* parser, StatsManager& statsManager) {
	// string pointing to an HTTP server (DNS name or IP)
	
	// variables for timing operations
	hrc::time_point st;
	hrc::time_point en;
	double elapsed;
	
	//char str[] = "https://www.tamu.edu/about/index.html#global-presence";
	//char str [] = "128.194.135.72";
	//string url = "http://balkans.aljazeera.net/misljenja";
	//string url = "http://tamu.edu";
	//string url = "http://allybruener.com/"; // should give 2xx 
	//string url = "https://google.com"; // failed with invalid scheme
	//string url = "http://xyz.com:0/"; // failed with invalid port
	//string url = "http://goopoiopoipoiopxx.com"; // 
	const char* str = url.c_str();
	string urlCopy = url;
	//printf("URL: %s\n", str);

	URLParser urlParser;
	URL urlElems = urlParser.parseURL(url);
	if (!urlElems.isValid) {
		//printf("failed with invalid %s\n", urlElems.blameInvalidOn.c_str());
		return;
	}
	
	/*if (urlElems.host.empty()) {
		printf("Something went wrong\n");
	}*/
	//printf("host %s, port %d\n", urlElems.host.c_str(), urlElems.port);

	//printf("\t  Checking host uniqueness...");
	pair<unordered_set<string>::iterator, bool> resultHost = addHostToSeen(urlElems.host);
	//printf("%s\t%d\n", urlElems.host.c_str(), resultHost.second);
	if (!resultHost.second) {
		//printf("failed.\n");
		statsManager.incrementDuplicateHosts();
		return;
	}
	statsManager.incrementURLsWithUniqueHost();
	//printf("passed\n");

	// structure used in DNS lookups
	struct hostent* remote;

	// structure for connecting to server
	struct sockaddr_in server;

	//printf("\t  Doing DNS... ");
	st = hrc::now();        // get start time point

	// first assume that the string is an IP address
	DWORD IP = inet_addr(urlElems.host.c_str());
	
	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(urlElems.host.c_str())) == NULL)
		{
			//printf("failed with %d\n", WSAGetLastError());
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
	//printf("done in %.2f ms, found %s\n", elapsed, inet_ntoa(server.sin_addr));
	statsManager.incrementSuccessfulDNSLookups();

	//printf("\t  Checking IP uniqueness...");
	string ipaddress(inet_ntoa(server.sin_addr));
	DWORD ip_dw = inet_addr(ipaddress.c_str());
	pair<unordered_set<DWORD>::iterator, bool> resultIP = addIPtoSeen(ip_dw);
	if (!resultIP.second) {
		//printf("failed\n");
		return;
	}
	statsManager.incrementURLsWithUniqueIP();
	//printf("passed\n");
	
	// setup the port # and protocol type
	server.sin_family = AF_INET;
	// host-to-network flips the byte order
	// TODO check if we need to find next open port for later assignment
	server.sin_port = htons(urlElems.port);
	
	string request = "/robots.txt";
	string httpMethod = "HEAD";
	string type = "robot";
	int maxDownloadSize = KB(16);
	
	
	
	if (!connectAndProcess(type, server, urlElems, request, httpMethod, 400, 499, maxDownloadSize, parser, statsManager)) {
		statsManager.incrementRobotReqFail();
		{//remove this
			lock_guard<mutex> lck(mutex_print);
			//printf("%s\t%d\t%d\n", urlElems.host.c_str(), urlElems.statusCode, urlElems.errorCode); //remove this
			this_thread::sleep_for(chrono::milliseconds(1));
		}
		return;
	}
	{//remove this
		lock_guard<mutex> lck(mutex_print);
		//printf("%s\t%d\n", urlElems.host.c_str(), urlElems.statusCode); //remove this
		this_thread::sleep_for(chrono::milliseconds(1));
	}
	statsManager.incrementURLsWhichPassedRobotCheck();

	request = urlElems.path;
	if (!urlElems.query.empty()) {
		request = request + "?" + urlElems.query;
	}
	httpMethod = "GET";
	type = "page";
	maxDownloadSize = MB(2);
	
	connectAndProcess(type, server, urlElems, request, httpMethod, 200, 299, maxDownloadSize, parser, statsManager);

}

bool WebClient::connectAndProcess(string type, struct sockaddr_in server, 
	URL& urlElems, string request, string httpMethod, int minAllowedStatusCode, int maxAllowedStatusCode, int maxDownloadSize, HTMLParserBase* parser, StatsManager& statsManager)
{
	hrc::time_point st;
	hrc::time_point en;
	double elapsed;

	Socket webSocket;

	// open a TCP socket
	if (!webSocket.Open()) {
		//printf("failed as couldn't open socket\n");
		urlElems.statusCode = -2;
		urlElems.errorCode = WSAGetLastError();
		return false;
	}

	// connect to the server on the specified or default port
	/*if (type == "robot") {
		//printf("\t  Connecting on %s... ", type.c_str());
	}
	else {
		//printf("\t* Connecting on %s... ", type.c_str());
	}*/
	
	st = hrc::now();        // get start time point
	if (!webSocket.Connect(server)) {
		//printf("failed with %d\n", WSAGetLastError());
		// close the socket to this server; open again for the next one
		urlElems.statusCode = -3;
		urlElems.errorCode = WSAGetLastError();
		webSocket.Close();
		return false;
	}
	en = hrc::now();        // get end time point
	elapsed = ELAPSED_MS(st, en);
	//printf("done in %.2f ms\n", elapsed);
	//printf("Successfully connected to %s (%s) on port %d\n", str, inet_ntoa(server.sin_addr), ntohs(server.sin_port));

	// send HTTP requests here
	string httpRequest = httpMethod + " " + request + " HTTP/1.0\r\n" + "Host: " + urlElems.host + "\r\n" + "User-agent: rajcrawler/1.1\r\n" + "Connection: close\r\n\r\n";

	//cout << "Sending http request:\n" << httpRequest << endl;
	int requestLen = strlen(httpRequest.c_str());
	char* sendBuf = new char[requestLen + 1];
	strcpy_s(sendBuf, requestLen + 1, httpRequest.c_str());

	// send the request
	//printf("\t  Loading... ");
	st = hrc::now();        // get start time point
	if (!webSocket.Send(sendBuf, requestLen)) {
		//printf("failed with %d\n", WSAGetLastError());
		// close the socket to this server; open again for the next one
		urlElems.statusCode = -4;
		urlElems.errorCode = WSAGetLastError();
		webSocket.Close();
		return false;
	}

	// read from the socket
	RecvOutcome outcome = webSocket.Read(maxDownloadSize);
	if (!outcome.success) {
		if(!outcome.errorAlreadyShown){
			//printf("failed with %d on recv\n", WSAGetLastError());
		}
		// close the socket to this server; open again for the next one
		urlElems.statusCode = -5;
		urlElems.errorCode = WSAGetLastError();
		webSocket.Close();
		return false;
	}

	char* recvBuf = webSocket.GetBufferData();
	if (recvBuf == NULL) {
		//printf("failed as no data found in buffer\n");
		// close the socket to this server; open again for the next one
		urlElems.statusCode = -6;
		urlElems.errorCode = WSAGetLastError();
		webSocket.Close();
		return false;
	}
	// close the socket to this server; open again for the next one
	webSocket.Close();

	HttpResponseParser httpReponseParser;
	HTTPresponse response = httpReponseParser.parseHttpResponse(recvBuf);

	// check for invalid response
	if (!response.isValid) {
		//printf("failed with non-HTTP header\n");
		urlElems.statusCode = -7;
		urlElems.errorCode = WSAGetLastError();
		return false;
	}

	en = hrc::now();        // get end time point
	elapsed = ELAPSED_MS(st, en);
	//printf("done in %.2f ms with %zu bytes\n", elapsed, strlen(recvBuf));
	
	int numBytes = strlen(recvBuf);
	if (type == "robot") {
		statsManager.incrementNumRobotBytes(numBytes);
	}
	else {
		statsManager.incrementNumPageBytes(numBytes);
		statsManager.incrementURLsWithValidHTTPcode();
	}
	
	if (type == "robot") { //remove this
		urlElems.statusCode = response.statusCode;
	}

	// status code in acceptable range
	if (response.statusCode >= minAllowedStatusCode && response.statusCode <= maxAllowedStatusCode) {
		if (type == "robot") {
			return true;
		}
		
		statsManager.incrementNumCode2xx();

		//printf("\t+ Parsing page... ");
		st = hrc::now();        // get start time point		
		// create new parser object
		//HTMLParserBase* parser = new HTMLParserBase;
		int nLinks;
		char* responseObjStr = new char[response.object.length() + 1];
		strcpy_s(responseObjStr, response.object.length() + 1, response.object.c_str());

		string baseUrl;
		if (urlElems.host.substr(0, 4).compare("www.") == 0) {
			baseUrl = "http://" + urlElems.host;
		}
		else {
			baseUrl = "http://www." + urlElems.host;
		}
		
		char* baseUrlstr = new char[baseUrl.length() + 1];
		strcpy_s(baseUrlstr, baseUrl.length() + 1, baseUrl.c_str());

		char* linkBuffer = parser->Parse(responseObjStr, strlen(responseObjStr), baseUrlstr, (int)strlen(baseUrlstr), &nLinks);

		// check for errors indicated by negative values
		if (nLinks < 0) {
			nLinks = 0;
		} 
		statsManager.incrementLinksFound(nLinks);
		en = hrc::now();        // get end time point
		elapsed = ELAPSED_MS(st, en);
		//printf("done in %.2f ms with %d links\n\n", elapsed, nLinks);
	
		// print each URL; these are NULL-separated C strings
		bool pageContainsTamuLink = false;
		
		for (int i = 0; i < nLinks; i++)
		{
			string pageLink(linkBuffer);
			
			URLParser urlParser2;
			string linkHost = urlParser2.getHost(pageLink);

			size_t found = linkHost.find("tamu.edu");
			if (found != string::npos) {
				statsManager.incrementNumLinksContainingTAMUAnywhere();
			}

			if (isHostTamu(linkHost)) {
				statsManager.incrementNumTAMUlinks();
				statsManager.incrementNumPagesContainingTamuLink();
				pageContainsTamuLink = true;
				break;
			}
			
			linkBuffer += strlen(linkBuffer) + 1;
		}
		
		if (pageContainsTamuLink) {
			string cleanPageHost = urlElems.host;
			if (cleanPageHost.substr(0, 4).compare("www.") == 0) {
				cleanPageHost = cleanPageHost.substr(4);
			}
			// how many links to a tamu.edu page originate from outside of TAMU i.e. with non abc.tamu.edu hosts
			if (!isHostTamu(cleanPageHost)) {
				statsManager.incrementNumLinksFromOutsideTAMU();
				statsManager.incrementNumPagesFromOutsideTamu();
			}
		}
	}
	else if (type == "robot") {
		return false;
	}
	else {
		if (response.statusCode >= 300 and response.statusCode <= 399) {
			statsManager.incrementNumCode3xx();
		}
		else if (response.statusCode >= 400 and response.statusCode <= 499) {
			statsManager.incrementNumCode4xx();
		}
		else if (response.statusCode >= 500 and response.statusCode <= 599) {
			statsManager.incrementNumCode5xx();
		}
		else {
			statsManager.incrementNumCodeOther();
		}
	}

	//printf("----------------------------------------\n");
	//printf("%s\n", response.header.c_str());

	return true;
}

