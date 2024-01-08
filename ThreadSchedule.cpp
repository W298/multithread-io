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
std::unordered_map<UINT, FileLock> g_fileLockMap;

DWORD WINAPI ThreadSchedule::ThreadFunc(LPVOID param)
{
	marker_series workerSeries(std::to_wstring(GetCurrentThreadId()).c_str());
	const HANDLE iocpHandle = *(HANDLE*)(param);

	DWORD ret;
	ULONG_PTR key;
	LPOVERLAPPED lpov;

	while (true)
	{
		workerSeries.write_flag(1, _T("Waiting Task..."));
		GetQueuedCompletionStatus(iocpHandle, &ret, &key, &lpov, INFINITE);
		
		if (key == g_exitCode)
			return 0;

		ThreadTaskArgs* args = (ThreadTaskArgs*)lpov;
		UINT fid = args->FID;
		
		switch (key)
		{

		case THREAD_TASK_READ_CALL:
		{
			DWORD waitResult = WaitForSingleObject(g_fileLockMap[fid].sem1, 0L);
			if (waitResult == WAIT_TIMEOUT)
			{
				workerSeries.write_flag(2, _T("Task Ignored"));
				break;
			}
			span* s = new span(workerSeries, 0, _T("Read Call Task"));
			{
				const std::wstring path = L"dummy\\";
				LARGE_INTEGER fileByteSize;
				DWORD alignedFileByteSize;

				auto fileHandle = g_fileHandleMap.find(fid);
				if (fileHandle == g_fileHandleMap.end())
				{
					g_fileHandleMap[fid] = CreateFileW((path + std::to_wstring(fid)).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
					g_fileIocpMap[fid] = CreateIoCompletionPort(g_fileHandleMap[fid], NULL, 0, g_threadCount);

					GetFileSizeEx(g_fileHandleMap[fid], &fileByteSize);
					alignedFileByteSize = (fileByteSize.QuadPart / 512u) * 512u;

					g_fileBufferMap[fid] = (BYTE*)VirtualAlloc(NULL, alignedFileByteSize, MEM_COMMIT, PAGE_READWRITE);
				}

				OVERLAPPED ov = { 0 };
				if (ReadFile(g_fileHandleMap[fid], g_fileBufferMap[fid], alignedFileByteSize, NULL, &ov) == FALSE && GetLastError() != ERROR_IO_PENDING)
				{
					ExitProcess(-1);
				}

				InterlockedIncrement(&g_fileLockMap[fid].status);	// FILE_STATUS_NON_ACCESS -> FILE_STATUS_READ_FILE_CALLED
				workerSeries.write_flag(3, _T("FILE_STATUS_READ_FILE_CALLED"));
			}
			delete s;
			ReleaseSemaphore(g_fileLockMap[fid].sem2, 1, NULL);
			break;
		}

		case THREAD_TASK_COMPLETION:
		{
			WaitForSingleObject(g_fileLockMap[fid].sem2, INFINITE);
			span* s = new span(workerSeries, 1, _T("Completion Task"));
			{
				DWORD fRet;
				ULONG_PTR fKey;
				LPOVERLAPPED fLpov;
				GetQueuedCompletionStatus(g_fileIocpMap[fid], &fRet, &fKey, &fLpov, INFINITE);

				BYTE dependencyFileCount = g_fileBufferMap[fid][0];

				InterlockedIncrement(&g_fileLockMap[fid].status);	// FILE_STATUS_READ_FILE_CALLED -> FILE_STATUS_READ_COMPLETED
				workerSeries.write_flag(3, _T("FILE_STATUS_READ_COMPLETED"));
			}
			delete s;
			ReleaseSemaphore(g_fileLockMap[fid].sem3, 1, NULL);
			break;
		}

		case THREAD_TASK_COMPUTE:
		{
			WaitForSingleObject(g_fileLockMap[fid].sem3, INFINITE);
			span* s = new span(workerSeries, 2, _T("Compute Task"));
			{
				// Compute Processing.
				{
					float number = 1.5;
					while (number <= 200000000.0)
					{
						number *= number;
					}
				}

				// Optional!!
				VirtualFree(g_fileBufferMap[fid], 1024, MEM_RELEASE);
				CloseHandle(g_fileIocpMap[fid]);
				CloseHandle(g_fileHandleMap[fid]);

				InterlockedIncrement(&g_fileLockMap[fid].status);	// FILE_STATUS_READ_COMPLETED -> FILE_STATUS_COMPUTE_COMPLETED
				workerSeries.write_flag(3, _T("FILE_STATUS_COMPUTE_COMPLETED"));
			}
			delete s;
			break;
		}

		default:
			break;

		}

		HeapFree(GetProcessHeap(), 0, args);
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

	// Create root file status objects.
	{
		g_fileLockMap[0] = FileLock(0);
	}

	// Send tasks to threads.
	span* sendSpan = new span(mainThreadSeries, _T("Send Tasks"));
	{
		ThreadTaskArgs* args = (ThreadTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadTaskArgs));
		args->FID = 0;

		if (PostQueuedCompletionStatus(g_threadIocpAry[0], 0, THREAD_TASK_READ_CALL, (LPOVERLAPPED)args) == FALSE)
		{
			ExitProcess(9);
		}
	}
	{
		ThreadTaskArgs* args = (ThreadTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadTaskArgs));
		args->FID = 0;

		if (PostQueuedCompletionStatus(g_threadIocpAry[1], 0, THREAD_TASK_COMPLETION, (LPOVERLAPPED)args) == FALSE)
		{
			ExitProcess(9);
		}
	}
	{
		ThreadTaskArgs* args = (ThreadTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadTaskArgs));
		args->FID = 0;

		if (PostQueuedCompletionStatus(g_threadIocpAry[2], 0, THREAD_TASK_COMPUTE, (LPOVERLAPPED)args) == FALSE)
		{
			ExitProcess(9);
		}
	}
	{
		ThreadTaskArgs* args = (ThreadTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadTaskArgs));
		args->FID = 0;

		if (PostQueuedCompletionStatus(g_threadIocpAry[0], 0, THREAD_TASK_READ_CALL, (LPOVERLAPPED)args) == FALSE)
		{
			ExitProcess(9);
		}
	}
	delete sendSpan;

	// Final tasks.
	for (int t = 0; t < g_threadCount; t++)
	{
		if (PostQueuedCompletionStatus(g_threadIocpAry[t], 0, g_exitCode, NULL) == FALSE)
		{
			ExitProcess(9);
		}
	}

	WaitForMultipleObjects(g_threadCount, g_threadHandleAry, TRUE, INFINITE);

	for (int t = 0; t < g_threadCount; t++)
	{
		CloseHandle(g_threadIocpAry[t]);
		CloseHandle(g_threadHandleAry[t]);
	}
}