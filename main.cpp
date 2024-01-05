#include <iostream>
#include <string>
#include <queue>
#include <windows.h>

using namespace std;

enum THREAD_TYPE
{
	GENERAL_NON_OVERLAP,
	GENERAL,
	CALL_ONLY,
	COMPLETION_ONLY,
	COMPUTE_ONLY,
	CALL_AND_COMPLETION,
	COMPLETION_AND_COMPUTE
};

enum THREAD_TASK_TYPE
{
	THREAD_TASK_READ_CALL,
	THREAD_TASK_READ_COMPLETION,
	THREAD_TASK_COMPUTE
};

struct ReadCallTaskArgs
{
	HANDLE fileHandle;
	size_t byteSizeToRead;
};

void ReadCallTask(LPVOID args)
{
	ReadCallTaskArgs* a = (ReadCallTaskArgs*)(args);

	cout << a->byteSizeToRead << "\n";
}

struct ThreadTask
{
	THREAD_TASK_TYPE taskType;
	LPVOID args;
};

class Thread
{
public:
	queue<ThreadTask> taskQueue;

	void ExecuteTask()
	{
		ThreadTask t = taskQueue.front();

		switch (t.taskType)
		{
		case THREAD_TASK_READ_CALL:
			ReadCallTask(t.args);
			break;
		}
	}
};

UINT g_threadCount = 8;

HANDLE CreateFileHandle(const string& fileName)
{
	return CreateFileA(fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
}

void DummyFileCreation(const string& fileName, size_t byteSize)
{
	string command = "fsutil file createnew " + fileName + " " + to_string(byteSize);
	system(command.c_str());
}

void DeleteDummyFiles()
{
	string command = "del dum*";
	system(command.c_str());
}

void Clear()
{
	DeleteDummyFiles();
}

int main()
{
	DummyFileCreation("dum", 2048);

	HANDLE h = CreateFileHandle("dum");

	Thread thread;

	ReadCallTaskArgs a = { h, 2048 };
	ThreadTask t1 = { THREAD_TASK_READ_CALL, &a };

	thread.taskQueue.push(t1);
	thread.ExecuteTask();
	
	Clear();

	return 0;
}