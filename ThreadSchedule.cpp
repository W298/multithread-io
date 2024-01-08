#include "pch.h"
#include "ThreadSchedule.h"
#include "cvmarkersobj.h"

using namespace Concurrency::diagnostic;
using namespace ThreadSchedule;

HANDLE g_iocp;
HANDLE g_threadHandleAry[g_threadCount];
HANDLE g_threadIocpAry[g_threadCount];

std::unordered_map<UINT, HANDLE> g_fileHandleMap;
std::unordered_map<UINT, HANDLE> g_fileIocpMap;
std::unordered_map<UINT, BYTE*> g_fileBufferMap;

DWORD WINAPI ThreadSchedule::ThreadFunc(LPVOID param)
{
	marker_series workerSeries(std::to_wstring(GetCurrentThreadId()).c_str());
	const HANDLE iocpHandle = *(HANDLE*)(param);

	DWORD ret;
	ULONG_PTR key;
	LPOVERLAPPED lpov;

	while (true)
	{
		workerSeries.write_alert(_T("Thread starts waiting task..."));
		GetQueuedCompletionStatus(iocpHandle, &ret, &key, &lpov, INFINITE);
		switch (key)
		{

		case THREAD_TASK_READ_CALL:
		{
			span* s = new span(workerSeries, 0, _T("Read Call Task"));
			ReadCallTaskArgs* readCallTaskArgs = (ReadCallTaskArgs*)lpov;

			const std::wstring path = L"dummy\\";
			UINT fid = readCallTaskArgs->FID;

			auto fileHandle = g_fileHandleMap.find(fid);
			if (fileHandle == g_fileHandleMap.end())
			{
				g_fileHandleMap[fid] = CreateFileW((path + std::to_wstring(fid)).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
				g_fileIocpMap[fid] = CreateIoCompletionPort(g_fileHandleMap[fid], NULL, 0, g_threadCount);
				g_fileBufferMap[fid] = (BYTE*)VirtualAlloc(NULL, readCallTaskArgs->ByteSizeToRead, MEM_COMMIT, PAGE_READWRITE);
			}

			OVERLAPPED ov = { 0 };
			if (ReadFile(g_fileHandleMap[fid], g_fileBufferMap[fid], readCallTaskArgs->ByteSizeToRead, NULL, &ov) == FALSE && GetLastError() != ERROR_IO_PENDING)
			{
				ExitProcess(-1);
			}

			HeapFree(GetProcessHeap(), 0, readCallTaskArgs);
			
			delete s;
			break;
		}

		case THREAD_TASK_COMPLETION:
		{
			span* s = new span(workerSeries, 1, _T("Completion Task"));
			CompletionTaskArgs* completionTaskArgs = (CompletionTaskArgs*)lpov;

			DWORD fRet;
			ULONG_PTR fKey;
			LPOVERLAPPED fLpov;
			GetQueuedCompletionStatus(g_fileIocpMap[completionTaskArgs->FID], &fRet, &fKey, &fLpov, INFINITE);

			BYTE dependencyFileCount = g_fileBufferMap[completionTaskArgs->FID][0];

			HeapFree(GetProcessHeap(), 0, completionTaskArgs);
			
			delete s;
			break;
		}

		case THREAD_TASK_COMPUTE:
		{
			span* s = new span(workerSeries, 2, _T("Compute Task"));
			ComputeTaskArgs* computeTaskArgs = (ComputeTaskArgs*)lpov;

			// Compute Processing.
			{
				float number = 1.5;
				while (number <= 200000000.0)
				{
					number *= number;
				}
			}
			
			// Optional!!
			VirtualFree(g_fileBufferMap[computeTaskArgs->FID], 1024, MEM_RELEASE);
			CloseHandle(g_fileIocpMap[computeTaskArgs->FID]);
			CloseHandle(g_fileHandleMap[computeTaskArgs->FID]);
			
			HeapFree(GetProcessHeap(), 0, computeTaskArgs);
			
			delete s;
			break;
		}

		case g_exitCode:
		{
			return 0;
		}

		default:
			break;

		}
	}

	return 0;
}

void ThreadSchedule::StartThreadTasks()
{
	marker_series mainThreadSeries(_T("Main Thread"));

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);

	// Create thread handles.
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

	// Send tasks to threads.
	span* sendSpan = new span(mainThreadSeries, _T("Send Tasks"));
	for (int t = 0; t < g_threadCount; t++)
	{
		{
			ReadCallTaskArgs* readCallTaskArgs = (ReadCallTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ReadCallTaskArgs));
			readCallTaskArgs->FID = t;
			readCallTaskArgs->ByteSizeToRead = 1024;

			if (PostQueuedCompletionStatus(g_threadIocpAry[t], 0, THREAD_TASK_READ_CALL, (LPOVERLAPPED)readCallTaskArgs) == FALSE)
			{
				ExitProcess(9);
			}
		}
		
		{
			CompletionTaskArgs* completionTaskArgs = (CompletionTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CompletionTaskArgs));
			completionTaskArgs->FID = t;

			if (PostQueuedCompletionStatus(g_threadIocpAry[t], 0, THREAD_TASK_COMPLETION, (LPOVERLAPPED)completionTaskArgs) == FALSE)
			{
				ExitProcess(9);
			}
		}
		
		{
			ComputeTaskArgs* computeTaskArgs = (ComputeTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ComputeTaskArgs));
			computeTaskArgs->FID = t;

			if (PostQueuedCompletionStatus(g_threadIocpAry[t], 0, THREAD_TASK_COMPUTE, (LPOVERLAPPED)computeTaskArgs) == FALSE)
			{
				ExitProcess(9);
			}
		}

		{
			if (PostQueuedCompletionStatus(g_threadIocpAry[t], 0, g_exitCode, NULL) == FALSE)
			{
				ExitProcess(9);
			}
		}
	}
	delete sendSpan;

	WaitForMultipleObjects(g_threadCount, g_threadHandleAry, TRUE, INFINITE);

	for (int t = 0; t < g_threadCount; t++)
	{
		CloseHandle(g_threadIocpAry[t]);
		CloseHandle(g_threadHandleAry[t]);
	}
}