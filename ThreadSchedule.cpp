#include "pch.h"
#include "ThreadSchedule.h"

#include <winternl.h>

using namespace Concurrency::diagnostic;
using namespace ThreadSchedule;

// Thread works.
HANDLE g_iocp;
HANDLE g_threadHandleAry[g_threadCount];
HANDLE g_threadIocpAry[g_threadCount];

// File cache data.
std::unordered_map<UINT, FileLock*> g_fileLockMap;
std::unordered_map<UINT, HANDLE> g_fileHandleMap;
std::unordered_map<UINT, HANDLE> g_fileIocpMap;
std::unordered_map<UINT, BYTE*> g_fileBufferMap;

HANDLE mut;

DWORD ThreadSchedule::GetAlignedByteSize(PLARGE_INTEGER fileByteSize, DWORD sectorSize)
{
	return ((fileByteSize->QuadPart / sectorSize) + 1u) * sectorSize;
}

void ThreadSchedule::ReadCallTaskWork(UINT fid)
{
	HANDLE fileHandle = CreateFileW((L"dummy\\" + std::to_wstring(fid)).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, g_fileFlag, NULL);
	HANDLE fileIOCP = CreateIoCompletionPort(fileHandle, NULL, 0, 0);

	LARGE_INTEGER fileByteSize;
	DWORD alignedFileByteSize;

	GetFileSizeEx(fileHandle, &fileByteSize);
	alignedFileByteSize = GetAlignedByteSize(&fileByteSize, 512u);

	BYTE* fileBuffer = (BYTE*)VirtualAlloc(NULL, alignedFileByteSize, MEM_COMMIT, PAGE_READWRITE);

	WaitForSingleObject(mut, INFINITE);
	g_fileHandleMap[fid] = fileHandle;
	g_fileIocpMap[fid] = fileIOCP;
	g_fileBufferMap[fid] = fileBuffer;
	ReleaseMutex(mut);

	OVERLAPPED ov = { 0 };
	if (ReadFile(fileHandle, fileBuffer, alignedFileByteSize, NULL, &ov) == FALSE && GetLastError() != ERROR_IO_PENDING)
		ExitProcess(-1);
}

void ThreadSchedule::CompletionTaskWork(UINT fid)
{
	DWORD fRet;
	ULONG_PTR fKey;
	LPOVERLAPPED fLpov;
	GetQueuedCompletionStatus(g_fileIocpMap[fid], &fRet, &fKey, &fLpov, INFINITE);

	BYTE dependencyFileCount = g_fileBufferMap[fid][0];
}

void ThreadSchedule::ComputeTaskWork(double timeOverMicroSeconds)
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

DWORD ThreadSchedule::HandleLockAcquireFailure(UINT fid, UINT threadTaskType)
{
	DWORD newWaitResult = WAIT_TIMEOUT;

	switch (threadTaskType)
	{

	case THREAD_TASK_READ_CALL:
	{
		switch (g_fileLockMap[fid]->status)
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

		break;
	}

	case THREAD_TASK_COMPLETION:
	{
		switch (g_fileLockMap[fid]->status)
		{
		case FILE_STATUS_READ_CALL_TASK_WAITING:
			// Read call task does not exist.
		case FILE_STATUS_READ_CALL_TASK_STARTED:
			// Read call task is running.
		case FILE_STATUS_READ_CALL_TASK_COMPLETED:
			// Read call task is already completed. But lock is not released yet.

			// 1. Wait read call task is completed.
			newWaitResult = WaitForSingleObject(g_fileLockMap[fid]->sem2, INFINITE);
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

		break;
	}

	case THREAD_TASK_COMPUTE:
	{
		switch (g_fileLockMap[fid]->status)
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
			newWaitResult = WaitForSingleObject(g_fileLockMap[fid]->sem3, INFINITE);
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

		break;
	}

	}

	return newWaitResult;
}

void ThreadSchedule::DoThreadTask(ThreadTaskArgs* args, UINT threadTaskType, marker_series* workerSeries, marker_series* statusSeries)
{
	UINT fid = args->FID;

	switch (threadTaskType)
	{

	case THREAD_TASK_READ_CALL:
	{
		// [VARIABLE]
		DWORD waitResult = WaitForSingleObject(g_fileLockMap[fid]->sem1, 0L);
		if (waitResult == WAIT_TIMEOUT)	// If failed to get lock...
		{
			waitResult = HandleLockAcquireFailure(fid, threadTaskType);
			if (waitResult == WAIT_TIMEOUT)
				break;
		}

		// FILE_STATUS_READ_CALL_TASK_WAITING -> FILE_STATUS_READ_CALL_TASK_STARTED
		InterlockedIncrement(&g_fileLockMap[fid]->status);
		statusSeries->write_flag(5, _T("FILE_STATUS_READ_CALL_TASK_WAITING -> FILE_STATUS_READ_CALL_TASK_STARTED"));

		span* s = new span(*workerSeries, 2, _T("Read Call Task"));
		ReadCallTaskWork(fid);
		delete s;

		// FILE_STATUS_READ_CALL_TASK_STARTED -> FILE_STATUS_READ_CALL_TASK_COMPLETED
		InterlockedIncrement(&g_fileLockMap[fid]->status);
		statusSeries->write_flag(5, _T("FILE_STATUS_READ_CALL_TASK_STARTED -> FILE_STATUS_READ_CALL_TASK_COMPLETED"));

		ReleaseSemaphore(g_fileLockMap[fid]->sem2, 1, NULL);

		// FILE_STATUS_READ_CALL_TASK_COMPLETED -> FILE_STATUS_COMPLETION_TASK_WAITING
		InterlockedIncrement(&g_fileLockMap[fid]->status);
		statusSeries->write_flag(5, _T("FILE_STATUS_READ_CALL_TASK_COMPLETED -> FILE_STATUS_COMPLETION_TASK_WAITING"));
		break;
	}

	case THREAD_TASK_COMPLETION:
	{
		// [VARIABLE]
		DWORD waitResult = WaitForSingleObject(g_fileLockMap[fid]->sem2, 0L);
		if (waitResult == WAIT_TIMEOUT) // If failed to get lock...
		{
			waitResult = HandleLockAcquireFailure(fid, threadTaskType);
			if (waitResult == WAIT_TIMEOUT)
				break;
		}

		// FILE_STATUS_COMPLETION_TASK_WAITING -> FILE_STATUS_COMPLETION_TASK_STARTED
		InterlockedIncrement(&g_fileLockMap[fid]->status);
		statusSeries->write_flag(5, _T("FILE_STATUS_COMPLETION_TASK_WAITING -> FILE_STATUS_COMPLETION_TASK_STARTED"));

		span* s = new span(*workerSeries, 3, _T("Completion Task"));
		CompletionTaskWork(fid);
		delete s;

		// FILE_STATUS_COMPLETION_TASK_STARTED -> FILE_STATUS_COMPLETION_TASK_COMPLETED
		InterlockedIncrement(&g_fileLockMap[fid]->status);
		statusSeries->write_flag(5, _T("FILE_STATUS_COMPLETION_TASK_STARTED -> FILE_STATUS_COMPLETION_TASK_COMPLETED"));

		ReleaseSemaphore(g_fileLockMap[fid]->sem3, 1, NULL);

		// FILE_STATUS_COMPLETION_TASK_COMPLETED -> FILE_STATUS_COMPUTE_TASK_WAITING
		InterlockedIncrement(&g_fileLockMap[fid]->status);
		statusSeries->write_flag(5, _T("FILE_STATUS_COMPLETION_TASK_COMPLETED -> FILE_STATUS_COMPUTE_TASK_WAITING"));
		break;
	}

	case THREAD_TASK_COMPUTE:
	{
		// [VARIABLE]
		DWORD waitResult = WaitForSingleObject(g_fileLockMap[fid]->sem3, 0L);
		if (waitResult == WAIT_TIMEOUT) // If failed to get lock...
		{
			waitResult = HandleLockAcquireFailure(fid, threadTaskType);
			if (waitResult == WAIT_TIMEOUT)
				break;
		}

		// FILE_STATUS_COMPUTE_TASK_WAITING -> FILE_STATUS_COMPUTE_TASK_STARTED
		InterlockedIncrement(&g_fileLockMap[fid]->status);
		statusSeries->write_flag(5, _T("FILE_STATUS_COMPUTE_TASK_WAITING -> FILE_STATUS_COMPUTE_TASK_STARTED"));

		span* s = new span(*workerSeries, 4, _T("Compute Task"));
		ComputeTaskWork(500.0);
		delete s;

		// FILE_STATUS_COMPUTE_TASK_STARTED -> FILE_STATUS_COMPUTE_TASK_COMPLTED
		InterlockedIncrement(&g_fileLockMap[fid]->status);
		statusSeries->write_flag(5, _T("FILE_STATUS_COMPUTE_TASK_STARTED -> FILE_STATUS_COMPUTE_TASK_COMPLTED"));

		break;
	}

	}

	HeapFree(GetProcessHeap(), 0, args);
}

DWORD WINAPI ThreadSchedule::ThreadFunc(LPVOID param)
{
	marker_series workerSeries(std::to_wstring(GetCurrentThreadId()).c_str());
	marker_series statusSeries((std::to_wstring(GetCurrentThreadId()) + L"-status").c_str());

	const HANDLE iocpHandle = *(HANDLE*)(param);

	DWORD ret;
	ULONG_PTR key;
	LPOVERLAPPED lpov;

	while (TRUE)
	{
		workerSeries.write_flag(1, _T("Waiting Task..."));
		GetQueuedCompletionStatus(iocpHandle, &ret, &key, &lpov, INFINITE);
		
		// If exit code received, terminate thread.
		if (key == g_exitCode)
			return 0;

		ThreadTaskArgs* args = (ThreadTaskArgs*)lpov;
		DoThreadTask(args, key, &workerSeries, &statusSeries);
	}

	return 0;
}

void ThreadSchedule::PostThreadTask(UINT t, UINT fid, UINT threadTaskType)
{
	ThreadTaskArgs* args = (ThreadTaskArgs*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadTaskArgs));
	args->FID = fid;

	if (FALSE == PostQueuedCompletionStatus(g_threadIocpAry[t], 0, threadTaskType, (LPOVERLAPPED)args))
		ExitProcess(9);
}

void ThreadSchedule::PostThreadExit(UINT t)
{
	if (FALSE == PostQueuedCompletionStatus(g_threadIocpAry[t], 0, g_exitCode, NULL))
		ExitProcess(9);
}

void ThreadSchedule::StartThreadTasks()
{
	marker_series mainThreadSeries(_T("Main Thread - ThreadSchedule"));

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);

	mut = CreateMutex(NULL, FALSE, _T("MUT"));

	// Create thread handles.
	for (int t = 0; t < g_threadCount; t++)
	{
		g_threadIocpAry[t] = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);

		DWORD tid;
		const HANDLE tHandle = CreateThread(NULL, 0, ThreadFunc, &g_threadIocpAry[t], 0, &tid);
		if (tHandle == NULL)
			ExitProcess(3);

		g_threadHandleAry[t] = tHandle;
	}

	// [VARIABLE] Create root file status objects.
	{
		for (int i = 0; i < testFileCount; i++)
		{
			g_fileLockMap[i] = new FileLock(i);
		}
	}

	// [VARIABLE] #Scenario. Main thread assign threads what to do.
	span* s = new span(mainThreadSeries, 1, _T("Send Tasks"));
	{
		for (int i = 0; i < testFileCount; i++)
		{
			PostThreadTask(i % g_threadCount, i, THREAD_TASK_READ_CALL);
		}
		for (int i = 0; i < testFileCount; i++)
		{
			PostThreadTask(i % g_threadCount, i, THREAD_TASK_COMPLETION);
		}
		for (int i = 0; i < testFileCount; i++)
		{
			PostThreadTask(i % g_threadCount, i, THREAD_TASK_COMPUTE);
		}
	}
	delete s;

	// Send exit code to threads.
	for (int t = 0; t < g_threadCount; t++)
		PostThreadExit(t);

	// Wait for thread termination.
	WaitForMultipleObjects(g_threadCount, g_threadHandleAry, TRUE, INFINITE);

	// [VARIABLE] Release cached datas.
	{
		for (int i = 0; i < testFileCount; i++)
		{
			VirtualFree(g_fileBufferMap[i], 0, MEM_RELEASE);
			CloseHandle(g_fileIocpMap[i]);
			CloseHandle(g_fileHandleMap[i]);
		}
		
		for (int f = 0; f < g_fileLockMap.size(); f++)
			delete g_fileLockMap[f];
		g_fileLockMap.clear();
	}

	// Release thread handle/iocp.
	for (int t = 0; t < g_threadCount; t++)
	{
		CloseHandle(g_threadIocpAry[t]);
		CloseHandle(g_threadHandleAry[t]);
	}
}