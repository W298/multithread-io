#pragma once

#include <string>

using namespace std;

HANDLE CreateFileHandle(const string& fileName)
{
	return CreateFileA(fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
}

void DummyFileCreation(const string& fileName, size_t byteSize)
{
	const string command = "fsutil file createnew " + fileName + " " + to_string(byteSize);
	system(command.c_str());
}

void DeleteDummyFiles()
{
	const string command = "del dum*";
	system(command.c_str());
}