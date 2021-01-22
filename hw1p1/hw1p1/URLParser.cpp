#include "pch.h"
#include "URLParser.h"

using namespace std;

// parse a url
URL URLParser::parseURL(string url) {
	URL urlElems;
	
	if (url.empty()) {
		printf("Error: Url is empty\n");
		urlElems.isValid = false;
		return urlElems;
	}

	if (url.length() < 7 || url.substr(0, 7).compare("http://") != 0) {
		printf("Error: Invalid scheme\n");
		urlElems.isValid = false;
		return urlElems;
	}

	// remove the scheme "http://"
	url = url.substr(7);

	// exclude the fragment from the url and everything to its right
	size_t found = url.find("#");
	if (found != string::npos) {
		if (found + 1 < url.length()) {
			urlElems.fragment = url.substr(found + 1);
		}
		url = url.substr(0, found);
	}

	// Extract query and truncate
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

	found = url.find(":");
	if (found != string::npos) {
		if (found + 1 < url.length()) {
			urlElems.port = atoi(url.substr(found+1).c_str());
		}
		else {
			urlElems.port = 80;
			urlElems.portSpecified = false;
		}
		url = url.substr(0, found);
	}
	else {
		urlElems.port = 80;
		urlElems.portSpecified = false;
	}

	urlElems.host = url;

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