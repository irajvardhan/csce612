#include "pch.h"
#include "TestUrlParser.h"

using namespace std;

void test_urlParser() 
{
    vector<string> urls{ "http://google.com", "http://tamu.edu:8080/cs/in:dex.php?test=1#something", "http://tamu.edu?tes:t=1/blah", "http://tamu.edu?test=1/blah"};
    
    // add more urls
    urls.push_back("http://amazon.com");

    URLParser urlParser;

    // Print Strings stored in Vector
    for (int i = 0; i < urls.size(); i++) {
        cout << urls[i] << endl;
        URL urlELems = urlParser.parseURL(urls[i]);
        if (!urlELems.isValid) {
            printf("Invalid url\n");
        }
        urlParser.displayURL(urlELems);
        cout << "-------------" << endl;
    }

}