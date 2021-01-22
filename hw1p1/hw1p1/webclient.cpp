#include "pch.h"
#include "WebClient.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;





void client(void) {
	// string pointing to an HTTP server (DNS name or IP)
	//char str[] = "https://www.tamu.edu/about/index.html#global-presence";
	//char str [] = "128.194.135.72";
	char str[] = "www.tamu.edu";

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

	// first assume that the string is an IP address
	DWORD IP = inet_addr(str);
	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(str)) == NULL)
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

	// setup the port # and protocol type
	server.sin_family = AF_INET;
	server.sin_port = htons(80);		// host-to-network flips the byte order

	// connect to the server on port 80
	if (!webSocket.Connect(server)) {
		printf("Error: failed to connect to server\n");
		return;
	}

	printf("Successfully connected to %s (%s) on port %d\n", str, inet_ntoa(server.sin_addr), htons(server.sin_port));

	// send HTTP requests here
	string request = "";
	string host = "";
	string httpRequest = "GET " + request + "HTTP/1.0\r\n" + "User-agent: rajTAMUcrawler/1.0\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n";
	int requestLen = strlen(httpRequest.c_str());
	char* sendBuf = new char[requestLen + 1];
	strcpy_s(sendBuf, requestLen + 1, httpRequest.c_str());
	
	cout << "Log: sending request" << endl;

	// if send request failed
	if (!webSocket.Send(sendBuf, requestLen)) {
		printf("Error: Send request failed.\n");
		return;
	}

	// if read from socket failed
	if (!webSocket.Read()) {
		printf("Error: Failed to read from socket.\n");
		return;
	}
	
	char* recvBuf = webSocket.GetBufferData();
	if (recvBuf == NULL) {
		printf("No data found in buffer\n");
		return;
	}

	cout << "buffer data: " << recvBuf << endl;
	// create new parser object
	HTMLParserBase* parser = new HTMLParserBase;
	int nLinks;
	//char* linkBuffer = parser->Parse(recvBuf, fileSize, baseUrl, (int)strlen(baseUrl), &nLinks);

	// check for errors indicated by negative values
	/*if (nLinks < 0)
		nLinks = 0;
	
	printf("Found %d links:\n", nLinks);
	*/




	// close the socket to this server; open again for the next one
	webSocket.Close();

	// call cleanup when done with everything and ready to exit program
	WSACleanup();
}