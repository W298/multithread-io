#include "pch.h"
#include "Thread.h"
#include "cvmarkersobj.h"

using namespace Concurrency::diagnostic;
using namespace Thread;

HANDLE g_iocp;
HANDLE g_threadHandleAry[g_threadCount];
HANDLE g_threadIocpAry[g_threadCount];

DWORD WINAPI Thread::ThreadFunc(LPVOID param)
{
	marker_series threadSeries(_T("Thread"));
	const HANDLE iocpHandle = *(HANDLE*)(param);

	DWORD ret;
	ULONG_PTR key;
	LPOVERLAPPED lpov;

	threadSeries.write_flag(_T("Thread is ready and Waiting"));
	GetQueuedCompletionStatus(iocpHandle, &ret, &key, &lpov, INFINITE);

	ReadCallTaskArgs* readCallArgs = static_cast<ReadCallTaskArgs*>(lpov->Pointer);

	if (readCallArgs->FileHandle == INVALID_HANDLE_VALUE)
		readCallArgs->FileHandle = CreateFileW(readCallArgs->FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	const HANDLE fileIocp = CreateIoCompletionPort(readCallArgs->FileHandle, NULL, 0, 8);

	const LPVOID buffer = VirtualAlloc(NULL, readCallArgs->ByteSizeToRead, MEM_COMMIT, PAGE_READWRITE);
	OVERLAPPED ov = { 0 };

	threadSeries.write_flag(_T("ReadFile Call"));
	if (ReadFile(readCallArgs->FileHandle, buffer, readCallArgs->ByteSizeToRead, NULL, &ov) == FALSE && GetLastError() != ERROR_IO_PENDING)
	{
		ExitProcess(-1);
	}

	GetQueuedCompletionStatus(fileIocp, &ret, &key, &lpov, INFINITE);
	threadSeries.write_flag(_T("Compute Start"));
	{
		// Compute Processing.
		float number = 1.5;
		while (number <= 2000000.0)
		{
			number *= number;
		}

		Sleep(50);
	}

	VirtualFree(buffer, readCallArgs->ByteSizeToRead, MEM_RELEASE);
	// CloseHandle(readCallArgs->FileHandle);
	// HeapFree(GetProcessHeap(), 0, readCallArgs);

	return 0;
}

void Thread::StartThreadTasks()
{
	marker_series mainThreadSeries(_T("Main Thread"));

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);

	for (int t = 0; t < g_threadCount; t++)
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

	mainThreadSeries.write_flag(_T("Start Send Task"));
	for (int t = 0; t < g_threadCount; t++)
	{
		ReadCallTaskArgs* args = (ReadCallTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ReadCallTaskArgs));
		args->FileHandle = INVALID_HANDLE_VALUE;
		args->FileName = L"dum";
		args->ByteSizeToRead = 1024;

		OVERLAPPED ov;
		ov.Pointer = args;

		PostQueuedCompletionStatus(g_threadIocpAry[t], sizeof(ReadCallTaskArgs), THREAD_TASK_READ_CALL, &ov);
	}
	mainThreadSeries.write_flag(_T("End Send Task"));

	for (int t = 0; t < g_threadCount; t++)
	{
		WaitForSingleObject(g_threadHandleAry[t], INFINITE);
		CloseHandle(g_threadIocpAry[t]);
		CloseHandle(g_threadHandleAry[t]);
	}
}