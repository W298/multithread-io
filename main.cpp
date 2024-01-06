#include <iostream>
#include <windows.h>

#include "cvmarkersobj.h"
#include "Helper.h"
#include "Types.h"

#define THREAD_COUNT 8

using namespace Concurrency::diagnostic;

HANDLE g_threadHandleAry[THREAD_COUNT];
HANDLE g_iocp;
HANDLE g_threadIocpAry[THREAD_COUNT];

DWORD WINAPI ThreadFunc(LPVOID param)
{
	const HANDLE iocpHandle = *(HANDLE*)param;
	marker_series threadSeries(_T("Thread"));

	DWORD ret;
	ULONG_PTR key;
	LPOVERLAPPED lpov;

	GetQueuedCompletionStatus(iocpHandle, &ret, &key, &lpov, INFINITE);

	threadSeries.write_flag(_T("Compute Request 01"));
	ReadCallTaskArgs* readCallArgs = static_cast<ReadCallTaskArgs*>(lpov->Pointer);
		readCallArgs->FileHandle = (HANDLE)20;

	float number = 1.5;
	while (true)
	{
		number *= number;
	}

	return 0;
}

int main()
{
	marker_series mainThreadSeries(_T("Main Thread"));

	DummyFileCreation("dum", 2048);

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, THREAD_COUNT);

	for (int t = 0; t < THREAD_COUNT; t++)
	{
		g_threadIocpAry[t] = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);

		DWORD tid;
		const HANDLE tHandle = CreateThread(NULL, 0, ThreadFunc, &g_threadIocpAry[t], 0, &tid);
		if (tHandle == NULL)
		{
			ExitProcess(3);
		}

		g_threadHandleAry[t] = tHandle;
	}

	for (int t = 0; t < THREAD_COUNT; t++)
	{
		ReadCallTaskArgs* args = (ReadCallTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ReadCallTaskArgs));
		args->FileHandle = INVALID_HANDLE_VALUE;
		args->FileName = L"dum";
		args->ByteSizeToRead = t;

		OVERLAPPED ov;
		ov.Pointer = args;

		mainThreadSeries.write_flag(_T("Send Request"));
		PostQueuedCompletionStatus(g_threadIocpAry[t], sizeof(ReadCallTaskArgs), THREAD_TASK_READ_CALL, &ov);
	}

	for (int t = 0; t < THREAD_COUNT; t++)
	{
		WaitForSingleObject(g_threadHandleAry[t], INFINITE);
		CloseHandle(g_threadIocpAry[t]);
		CloseHandle(g_threadHandleAry[t]);
	}

	DeleteDummyFiles();

	return 0;
}