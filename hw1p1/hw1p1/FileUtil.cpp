/*
* CSCE 612 [Spring 2021]
* by Raj Vardhan
*/
#include "pch.h"
#include "FileUtil.h"

#include <stdio.h>
#include <Windows.h>
using namespace std;

// Code reference from https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c/33486052#33486052
bool doesFileExist(string filename) {
	ifstream f(filename.c_str());
	return f.good();
}

// Code reference taken from https://stackoverflow.com/questions/2912520/read-file-contents-into-a-string-in-c/2912614#2912614

string readFileToString(string filename) {
	ifstream ifs(filename);
	string fileContent((istreambuf_iterator<char>(ifs)),
		(istreambuf_iterator<char>()));
	return fileContent;
}


string readFileToString2(string filename) {
	// open file
	/*int filename_len = strlen(filename.c_str());
	char* fname = new char[filename_len + 1];
	strcpy_s(fname, filename_len + 1, filename.c_str());
	*/
	//char myfilename[] = "URL-input.txt"; //tamu2018.html
	HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL, NULL);
	//HANDLE hFile = CreateFile(myfilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	// process errors
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("CreateFile failed with %d\n", GetLastError());
		return "";
	}

	// get file size
	LARGE_INTEGER li;
	BOOL bRet = GetFileSizeEx(hFile, &li);
	// process errors
	if (bRet == 0)
	{
		printf("GetFileSizeEx error %d\n", GetLastError());
		return "";
	}

	// read file into a buffer
	int fileSize = (DWORD)li.QuadPart;			// assumes file size is below 2GB; otherwise, an __int64 is needed
	DWORD bytesRead;
	// allocate buffer
	char* fileBuf = new char[fileSize+1];
	// read into the buffer
	bRet = ReadFile(hFile, fileBuf, fileSize, &bytesRead, NULL);
	// process errors
	if (bRet == 0 || bytesRead != fileSize)
	{
		printf("ReadFile failed with %d\n", GetLastError());
		return "";
	}
	fileBuf[fileSize] = '\0';
	// done with the file
	CloseHandle(hFile);

	string contents(fileBuf);
	return contents;
}