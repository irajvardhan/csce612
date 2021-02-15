/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "URLParser.h"

using namespace std;

// parse a url
URL URLParser::parseURL(string url) {
	URL urlElems;
	try {
		//printf("\t  Parsing URL... ");
		if (url.empty()) {
			urlElems.blameInvalidOn = "url_size";
			throw "Error: Url is empty\n";
		}

		if (url.length() < 7 || url.substr(0, 7).compare("http://") != 0 || url.substr(0, 8).compare("https") == 0) {
			urlElems.blameInvalidOn = "scheme";
			throw "failed due to invalid scheme";
		}

		if (url.length() > CRAWLER_MAX_URL_LEN) {
			urlElems.blameInvalidOn = "url_length";
			throw "URL exceeded max URL length";
		}

		// remove the scheme "http://"
		url = url.substr(7);

		// remove "www." if present
		/*
		if (url.substr(0, 4).compare("www.") == 0) {
			url = url.substr(4);
		}*/

		// exclude the fragment from the url and everything to its right
		size_t found = url.find("#");
		if (found != string::npos) {
			if (found + 1 < url.length()) {
				urlElems.fragment = url.substr(found + 1);
			}
			url = url.substr(0, found);
		}

		// Extract query and truncate
		urlElems.query = "";
		found = url.find("?");
		if (found != string::npos) {
			// // query is not empty
			if (found + 1 < url.length()) {
				urlElems.query = url.substr(found + 1);
			}
			// the "?" and everything to its right will be removed
			url = url.substr(0, found);
		}

		// Extract path and truncate
		found = url.find("/");
		if (found != string::npos) {
			urlElems.path = url.substr(found);
			url = url.substr(0, found);
		}
		else {
			urlElems.path = "/";
		}

		// set default values of port and portSpecified
		urlElems.port = 80;
		urlElems.portSpecified = false;
		found = url.find(":");
		if (found != string::npos) {

			if (found + 1 < url.length()) {
				int port = atoi(url.substr(found + 1).c_str());
				if (port <= 0 || port >= 65354) {
					urlElems.blameInvalidOn = "port";
					throw "failed with invalid port\n";
				}
				else {
					urlElems.port = port;
					urlElems.portSpecified = true;
				}
			}
			else {
				urlElems.blameInvalidOn = "port";
				throw "failed with invalid port\n";
			}
			url = url.substr(0, found);
		}

		urlElems.host = url;
		
		if (urlElems.host.length() > CRAWLER_MAX_HOST_LEN) {
			urlElems.blameInvalidOn = "url_host_length";
			throw "URL's host exceeded max host length";
		}
		
		urlElems.isValid = true;
	}
	catch (const char* msg) {
		urlElems.isValid = false;
	}

	return urlElems;
}

// display a URL
void URLParser::displayURL(URL urlElems) {

	if (!urlElems.scheme.empty()) {
		cout << "scheme: " << urlElems.scheme << endl;
	}

	if (!urlElems.host.empty()) {
		cout << "host: " << urlElems.host << endl;
	}

	cout << "port: " << urlElems.port << endl;
	cout << "portSpecified: ";
	if (urlElems.portSpecified)
		cout << "yes " << endl;
	else
		cout << "no " << endl;

	if (!urlElems.scheme.empty()) {
		cout << "scheme: " << urlElems.scheme << endl;
	}

	cout << "path: " << urlElems.path << endl;
	

	if (!urlElems.query.empty()) {
		cout << "query: " << urlElems.query << endl;
	}

	if (!urlElems.fragment.empty()) {
		cout << "fragment: " << urlElems.fragment << endl;
	}

}

// parse a url
string URLParser::getHost(string url) {
	string host = "";
	try {
		//printf("\t  Parsing URL... ");
		if (url.empty()) {
			throw "Error: Url is empty\n";
		}

		// remove the scheme "http://"
		if (url.substr(0, 7).compare("http://") == 0) {
			url = url.substr(7);
		}
		else if (url.substr(0, 8).compare("https://") == 0) {
			url = url.substr(8);
		}

		if (url.substr(0, 4).compare("www.") == 0) {
			url = url.substr(4);
		}

		// exclude the fragment from the url and everything to its right
		size_t found = url.find("#");
		if (found != string::npos) {
			url = url.substr(0, found);
		}

		// Extract query and truncate
		found = url.find("?");
		if (found != string::npos) {
			url = url.substr(0, found);
		}

		// Extract path and truncate
		found = url.find("/");
		if (found != string::npos) {
			url = url.substr(0, found);
		}
		
		found = url.find(":");
		if (found != string::npos) {
			url = url.substr(0, found);
		}

		host = url;
	}
	catch (const char* msg) {
		
	}

	return host;
}