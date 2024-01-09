#include "pch.h"
#include "ThreadSchedule.h"

using namespace Concurrency::diagnostic;
using namespace ThreadSchedule;

HANDLE g_iocp;
HANDLE g_threadHandleAry[g_threadCount];
HANDLE g_threadIocpAry[g_threadCount];

std::unordered_map<UINT, HANDLE> g_fileHandleMap;
std::unordered_map<UINT, HANDLE> g_fileIocpMap;
std::unordered_map<UINT, BYTE*> g_fileBufferMap;
std::unordered_map<UINT, FileLock> g_fileLockMap;

void ThreadSchedule::ComputeFunc(double timeOverMicroSeconds)
{
	LARGE_INTEGER freq, start, end;
	double elapsedMicroSeconds = 0.0;
	
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);

	while (elapsedMicroSeconds <= timeOverMicroSeconds)
	{
		int num = 1, primes = 0;
		for (num = 1; num <= 5; num++) {			
			int i = 2;
			while (i <= num) {
				QueryPerformanceCounter(&end);
				elapsedMicroSeconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart) * 1000.0 * 1000.0;
				if (elapsedMicroSeconds > timeOverMicroSeconds)
					return;
				
				if (num % i == 0)
					break;
				i++;
			}
			if (i == num)
				primes++;
		}
	}

	return;
}

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
			// [VARIABLE]
			DWORD waitResult = WaitForSingleObject(g_fileLockMap[fid].sem1, 0L);
			if (waitResult == WAIT_TIMEOUT)	// If failed to get lock...
			{
				switch (g_fileLockMap[fid].status)
				{

				case FILE_STATUS_READ_CALL_TASK_WAITING:
					// Someone takes lock, but status is not updated yet.
					// It must be FILE_STATUS_READ_CALL_TASK_STARTED.
					// So do the same task as FILE_STATUS_READ_CALL_TASK_STARTED.
				case FILE_STATUS_READ_CALL_TASK_STARTED:
					// Read call task is running.
				case FILE_STATUS_READ_CALL_TASK_COMPLETED:
					// Read call task is already completed. But lock is not released yet.
				case FILE_STATUS_COMPLETION_TASK_WAITING:
					// Completion task does not exist.
				case FILE_STATUS_COMPLETION_TASK_STARTED:
					// Completion task is running.
				case FILE_STATUS_COMPLETION_TASK_COMPLETED:
					// Completion task is already completed. But lock is not released yet.
				case FILE_STATUS_COMPUTE_TASK_WAITING:
					// Compute task does not exist.
				case FILE_STATUS_COMPUTE_TASK_STARTED:
					// Compute task is running.
				case FILE_STATUS_COMPUTE_TASK_COMPLTED:
					// Compute task is already completed. Currently we'll not release locks.
					break;
				}

				if (waitResult == WAIT_TIMEOUT)
					break;
			}
			
			// FILE_STATUS_READ_CALL_TASK_WAITING -> FILE_STATUS_READ_CALL_TASK_STARTED
			InterlockedIncrement(&g_fileLockMap[fid].status);
			workerSeries.write_flag(5, _T("FILE_STATUS_READ_CALL_TASK_WAITING -> FILE_STATUS_READ_CALL_TASK_STARTED"));
			
			span* s = new span(workerSeries, 2, _T("Read Call Task"));
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
			}
			delete s;

			// FILE_STATUS_READ_CALL_TASK_STARTED -> FILE_STATUS_READ_CALL_TASK_COMPLETED
			InterlockedIncrement(&g_fileLockMap[fid].status);
			workerSeries.write_flag(5, _T("FILE_STATUS_READ_CALL_TASK_STARTED -> FILE_STATUS_READ_CALL_TASK_COMPLETED"));

			ReleaseSemaphore(g_fileLockMap[fid].sem2, 1, NULL);
			
			// FILE_STATUS_READ_CALL_TASK_COMPLETED -> FILE_STATUS_COMPLETION_TASK_WAITING
			InterlockedIncrement(&g_fileLockMap[fid].status);
			workerSeries.write_flag(5, _T("FILE_STATUS_READ_CALL_TASK_COMPLETED -> FILE_STATUS_COMPLETION_TASK_WAITING"));
			break;
		}

		case THREAD_TASK_COMPLETION:
		{
			// [VARIABLE]
			DWORD waitResult = WaitForSingleObject(g_fileLockMap[fid].sem2, 0L);
			if (waitResult == WAIT_TIMEOUT) // If failed to get lock...
			{
				switch (g_fileLockMap[fid].status)
				{
				case FILE_STATUS_READ_CALL_TASK_WAITING:
					// Read call task does not exist.
				case FILE_STATUS_READ_CALL_TASK_STARTED:
					// Read call task is running.
				case FILE_STATUS_READ_CALL_TASK_COMPLETED:
					// Read call task is already completed. But lock is not released yet.
					
					// 1. Wait read call task is completed.
					waitResult = WaitForSingleObject(g_fileLockMap[fid].sem2, INFINITE);
					break;

				case FILE_STATUS_COMPLETION_TASK_WAITING:
					// Someone takes lock, but status is not updated yet.
					// It must be FILE_STATUS_COMPLETION_TASK_STARTED.
					// So do the same job as FILE_STATUS_COMPLETION_TASK_STARTED.
				case FILE_STATUS_COMPLETION_TASK_STARTED:
					// Completion task is running.
				case FILE_STATUS_COMPLETION_TASK_COMPLETED:
					// Completion task is already completed. But lock is not released yet.
				case FILE_STATUS_COMPUTE_TASK_WAITING:
					// Compute task dose not exist.
				case FILE_STATUS_COMPUTE_TASK_STARTED:
					// Compute task is running.
				case FILE_STATUS_COMPUTE_TASK_COMPLTED:
					// Compute task is already completed. Currently we'll not release locks.
					break;
				}

				if (waitResult == WAIT_TIMEOUT)
					break;
			}

			// FILE_STATUS_COMPLETION_TASK_WAITING -> FILE_STATUS_COMPLETION_TASK_STARTED
			InterlockedIncrement(&g_fileLockMap[fid].status);
			workerSeries.write_flag(5, _T("FILE_STATUS_COMPLETION_TASK_WAITING -> FILE_STATUS_COMPLETION_TASK_STARTED"));

			span* s = new span(workerSeries, 3, _T("Completion Task"));
			{
				DWORD fRet;
				ULONG_PTR fKey;
				LPOVERLAPPED fLpov;
				GetQueuedCompletionStatus(g_fileIocpMap[fid], &fRet, &fKey, &fLpov, INFINITE);

				BYTE dependencyFileCount = g_fileBufferMap[fid][0];
			}
			delete s;

			// FILE_STATUS_COMPLETION_TASK_STARTED -> FILE_STATUS_COMPLETION_TASK_COMPLETED
			InterlockedIncrement(&g_fileLockMap[fid].status);
			workerSeries.write_flag(5, _T("FILE_STATUS_COMPLETION_TASK_STARTED -> FILE_STATUS_COMPLETION_TASK_COMPLETED"));

			ReleaseSemaphore(g_fileLockMap[fid].sem3, 1, NULL);

			// FILE_STATUS_COMPLETION_TASK_COMPLETED -> FILE_STATUS_COMPUTE_TASK_WAITING
			InterlockedIncrement(&g_fileLockMap[fid].status);
			workerSeries.write_flag(5, _T("FILE_STATUS_COMPLETION_TASK_COMPLETED -> FILE_STATUS_COMPUTE_TASK_WAITING"));
			break;
		}

		case THREAD_TASK_COMPUTE:
		{
			// [VARIABLE]
			DWORD waitResult = WaitForSingleObject(g_fileLockMap[fid].sem3, 0L);
			if (waitResult == WAIT_TIMEOUT) // If failed to get lock...
			{
				switch (g_fileLockMap[fid].status)
				{
				case FILE_STATUS_READ_CALL_TASK_WAITING:
					// Read call task does not exist. Completion task cannot exist also.
				case FILE_STATUS_READ_CALL_TASK_STARTED:
					// Read call task is running.
				case FILE_STATUS_READ_CALL_TASK_COMPLETED:
					// Read call task is already completed. But lock is not released yet.
				case FILE_STATUS_COMPLETION_TASK_WAITING:
					// Completion task does not exist.
				case FILE_STATUS_COMPLETION_TASK_STARTED:
					// Completion task is running.
				case FILE_STATUS_COMPLETION_TASK_COMPLETED:
					// Completion task is already completed. But lock is not released yet.
					
					// 1. Wait read call/completion task is completed.
					waitResult = WaitForSingleObject(g_fileLockMap[fid].sem3, INFINITE);
					break;

				case FILE_STATUS_COMPUTE_TASK_WAITING:
					// Someone takes lock, but status is not updated yet.
					// It must be FILE_STATUS_COMPUTE_TASK_STARTED.
					// So do the same task as FILE_STATUS_COMPUTE_TASK_STARTED.
				case FILE_STATUS_COMPUTE_TASK_STARTED:
					// Compute task is running.
				case FILE_STATUS_COMPUTE_TASK_COMPLTED:
					// Compute task is already completed. Currently we'll not release locks.
					break;
				}

				if (waitResult == WAIT_TIMEOUT)
					break;
			}

			// FILE_STATUS_COMPUTE_TASK_WAITING -> FILE_STATUS_COMPUTE_TASK_STARTED
			InterlockedIncrement(&g_fileLockMap[fid].status);
			workerSeries.write_flag(5, _T("FILE_STATUS_COMPUTE_TASK_WAITING -> FILE_STATUS_COMPUTE_TASK_STARTED"));

			span* s = new span(workerSeries, 4, _T("Compute Task"));
			{
				// Compute.
				ComputeFunc(100.0);
			}
			delete s;

			// FILE_STATUS_COMPUTE_TASK_STARTED -> FILE_STATUS_COMPUTE_TASK_COMPLTED
			InterlockedIncrement(&g_fileLockMap[fid].status);
			workerSeries.write_flag(5, _T("FILE_STATUS_COMPUTE_TASK_STARTED -> FILE_STATUS_COMPUTE_TASK_COMPLTED"));

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
	marker_series mainThreadSeries(_T("Main Thread - ThreadSchedule"));

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

	// [VARIABLE] Create root file status objects.
	{
		g_fileLockMap[0] = FileLock(0);
	}

	// [VARIABLE] #Scenario. Main thread assign threads what to do.
	span* sendSpan = new span(mainThreadSeries, 1, _T("Send Tasks"));
	{
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
		Sleep(1000);
		// After all task is done, trying to access the file.
		{
			ThreadTaskArgs* args = (ThreadTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadTaskArgs));
			args->FID = 0;

			if (PostQueuedCompletionStatus(g_threadIocpAry[0], 0, THREAD_TASK_READ_CALL, (LPOVERLAPPED)args) == FALSE)
			{
				ExitProcess(9);
			}
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

	// [VARIABLE] Release cached datas.
	{
		VirtualFree(g_fileBufferMap[0], 1024, MEM_RELEASE);
		CloseHandle(g_fileIocpMap[0]);
		CloseHandle(g_fileHandleMap[0]);
	}

	for (int t = 0; t < g_threadCount; t++)
	{
		CloseHandle(g_threadIocpAry[t]);
		CloseHandle(g_threadHandleAry[t]);
	}
}