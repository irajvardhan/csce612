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

	//return;//remove this

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

