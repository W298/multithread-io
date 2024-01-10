#include "pch.h"
#include "ThreadSchedule.h"

using namespace Concurrency::diagnostic;
using namespace ThreadSchedule;

// Thread works.
HANDLE g_iocp;
HANDLE g_threadHandleAry[g_threadCount];
HANDLE g_threadIocpAry[g_threadCount];

// Map write mutex handle.
HANDLE g_mapWriteMutex;

// File cache data.
std::unordered_map<UINT, FileLock*> g_fileLockMap;
std::unordered_map<UINT, HANDLE> g_fileHandleMap;
std::unordered_map<UINT, HANDLE> g_fileIocpMap;
std::unordered_map<UINT, BYTE*> g_fileBufferMap;

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

	WaitForSingleObject(g_mapWriteMutex, INFINITE);
	{
		g_fileHandleMap[fid] = fileHandle;
		g_fileIocpMap[fid] = fileIOCP;
		g_fileBufferMap[fid] = fileBuffer;
	}
	ReleaseMutex(g_mapWriteMutex);

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

void ThreadSchedule::ComputeTaskWork(UINT fid)
{
	LARGE_INTEGER freq, start, end;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	
	UINT timeOverMicroSeconds;
	memcpy(&timeOverMicroSeconds, g_fileBufferMap[fid], sizeof(UINT));

	float elapsedMicroSeconds = 0.0f;
	while (elapsedMicroSeconds <= timeOverMicroSeconds)
	{
		int num = 1, primes = 0;
		for (num = 1; num <= 5; num++) {
			int i = 2;
			while (i <= num) {
				QueryPerformanceCounter(&end);
				elapsedMicroSeconds = ((float)(end.QuadPart - start.QuadPart) / freq.QuadPart) * 1000.0f * 1000.0f;
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

	if (g_fileLockMap[fid]->status < threadTaskType * 3)
		newWaitResult = WaitForSingleObject(g_fileLockMap[fid]->sem[threadTaskType], INFINITE);

	//switch (threadTaskType)
	//{

	//case THREAD_TASK_READ_CALL:
	//{
	//	switch (g_fileLockMap[fid]->status)
	//	{
	//	case FILE_STATUS_READ_CALL_TASK_WAITING:
	//		// Someone takes lock, but status is not updated yet.
	//		// It must be FILE_STATUS_READ_CALL_TASK_STARTED.
	//		// So do the same task as FILE_STATUS_READ_CALL_TASK_STARTED.
	//	case FILE_STATUS_READ_CALL_TASK_STARTED:
	//		// Read call task is running.
	//	case FILE_STATUS_READ_CALL_TASK_COMPLETED:
	//		// Read call task is already completed. But lock is not released yet.
	//	case FILE_STATUS_COMPLETION_TASK_WAITING:
	//		// Completion task does not exist.
	//	case FILE_STATUS_COMPLETION_TASK_STARTED:
	//		// Completion task is running.
	//	case FILE_STATUS_COMPLETION_TASK_COMPLETED:
	//		// Completion task is already completed. But lock is not released yet.
	//	case FILE_STATUS_COMPUTE_TASK_WAITING:
	//		// Compute task does not exist.
	//	case FILE_STATUS_COMPUTE_TASK_STARTED:
	//		// Compute task is running.
	//	case FILE_STATUS_COMPUTE_TASK_COMPLTED:
	//		// Compute task is already completed. Currently we'll not release locks.
	//		break;
	//	}

	//	break;
	//}

	//case THREAD_TASK_COMPLETION:
	//{
	//	switch (g_fileLockMap[fid]->status)
	//	{
	//	case FILE_STATUS_READ_CALL_TASK_WAITING:
	//		// Read call task does not exist.
	//	case FILE_STATUS_READ_CALL_TASK_STARTED:
	//		// Read call task is running.
	//	case FILE_STATUS_READ_CALL_TASK_COMPLETED:
	//		// Read call task is already completed. But lock is not released yet.

	//		// 1. Wait read call task is completed.
	//		newWaitResult = WaitForSingleObject(g_fileLockMap[fid]->sem[1], INFINITE);
	//		break;

	//	case FILE_STATUS_COMPLETION_TASK_WAITING:
	//		// Someone takes lock, but status is not updated yet.
	//		// It must be FILE_STATUS_COMPLETION_TASK_STARTED.
	//		// So do the same job as FILE_STATUS_COMPLETION_TASK_STARTED.
	//	case FILE_STATUS_COMPLETION_TASK_STARTED:
	//		// Completion task is running.
	//	case FILE_STATUS_COMPLETION_TASK_COMPLETED:
	//		// Completion task is already completed. But lock is not released yet.
	//	case FILE_STATUS_COMPUTE_TASK_WAITING:
	//		// Compute task dose not exist.
	//	case FILE_STATUS_COMPUTE_TASK_STARTED:
	//		// Compute task is running.
	//	case FILE_STATUS_COMPUTE_TASK_COMPLTED:
	//		// Compute task is already completed. Currently we'll not release locks.
	//		break;
	//	}

	//	break;
	//}

	//case THREAD_TASK_COMPUTE:
	//{
	//	switch (g_fileLockMap[fid]->status)
	//	{
	//	case FILE_STATUS_READ_CALL_TASK_WAITING:
	//		// Read call task does not exist. Completion task cannot exist also.
	//	case FILE_STATUS_READ_CALL_TASK_STARTED:
	//		// Read call task is running.
	//	case FILE_STATUS_READ_CALL_TASK_COMPLETED:
	//		// Read call task is already completed. But lock is not released yet.
	//	case FILE_STATUS_COMPLETION_TASK_WAITING:
	//		// Completion task does not exist.
	//	case FILE_STATUS_COMPLETION_TASK_STARTED:
	//		// Completion task is running.
	//	case FILE_STATUS_COMPLETION_TASK_COMPLETED:
	//		// Completion task is already completed. But lock is not released yet.

	//		// 1. Wait read call/completion task is completed.
	//		newWaitResult = WaitForSingleObject(g_fileLockMap[fid]->sem[2], INFINITE);
	//		break;

	//	case FILE_STATUS_COMPUTE_TASK_WAITING:
	//		// Someone takes lock, but status is not updated yet.
	//		// It must be FILE_STATUS_COMPUTE_TASK_STARTED.
	//		// So do the same task as FILE_STATUS_COMPUTE_TASK_STARTED.
	//	case FILE_STATUS_COMPUTE_TASK_STARTED:
	//		// Compute task is running.
	//	case FILE_STATUS_COMPUTE_TASK_COMPLTED:
	//		// Compute task is already completed. Currently we'll not release locks.
	//		break;
	//	}

	//	break;
	//}

	//}

	return newWaitResult;
}

void ThreadSchedule::DoThreadTask(ThreadTaskArgs* args, UINT threadTaskType, marker_series* workerSeries)
{
	UINT fid = args->FID;

	// [VARIABLE]
	DWORD waitResult = WaitForSingleObject(g_fileLockMap[fid]->sem[threadTaskType], 0L);
	if (waitResult == WAIT_TIMEOUT)	// If failed to get lock...
	{
		waitResult = HandleLockAcquireFailure(fid, threadTaskType);
		if (waitResult == WAIT_TIMEOUT)
		{
			HeapFree(GetProcessHeap(), 0, args);
			return;
		}
	}

	InterlockedIncrement(&g_fileLockMap[fid]->status);

	span* s = new span(*workerSeries, threadTaskType, _T("Task"));
	switch (threadTaskType)
	{
	case THREAD_TASK_READ_CALL:
		ReadCallTaskWork(fid);
		break;
	case THREAD_TASK_COMPLETION:
		CompletionTaskWork(fid);
		break;
	case THREAD_TASK_COMPUTE:
		ComputeTaskWork(fid);
		break;
	}
	delete s;

	InterlockedIncrement(&g_fileLockMap[fid]->status);

	if (threadTaskType < THREAD_TASK_COMPUTE)
	{
		ReleaseSemaphore(g_fileLockMap[fid]->sem[threadTaskType + 1], 1, NULL);
		InterlockedIncrement(&g_fileLockMap[fid]->status);
	}

	HeapFree(GetProcessHeap(), 0, args);
}

DWORD WINAPI ThreadSchedule::ThreadFunc(LPVOID param)
{
	marker_series workerSeries(std::to_wstring(GetCurrentThreadId()).c_str());
	
	const HANDLE iocpHandle = *(HANDLE*)(param);

	OVERLAPPED_ENTRY ent[g_taskRemoveCount];
	ULONG entRemoved;

	while (TRUE)
	{
		workerSeries.write_flag(1, _T("Waiting Task..."));

		GetQueuedCompletionStatusEx(iocpHandle, ent, g_taskRemoveCount, &entRemoved, INFINITE, FALSE);

		for (int i = 0; i < entRemoved; i++)
		{
			ULONG_PTR key = ent[i].lpCompletionKey;
			LPOVERLAPPED lpov = ent[i].lpOverlapped;

			// If exit code received, terminate thread.
			if (key == g_exitCode)
				return 0;

			ThreadTaskArgs* args = (ThreadTaskArgs*)lpov;
			DoThreadTask(args, key, &workerSeries);
		}
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

void ThreadSchedule::StartThreadTasks(UINT totalFileCount)
{
	marker_series mainThreadSeries(_T("Main Thread - ThreadSchedule"));

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);
	g_mapWriteMutex = CreateMutex(NULL, FALSE, _T("Map Write Mutex"));

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
		for (int i = 0; i < totalFileCount; i++)
			g_fileLockMap[i] = new FileLock(i);
	}

	// [VARIABLE] #Scenario. Main thread assign threads what to do.
	span* s = new span(mainThreadSeries, 1, _T("Send Tasks"));
	{
		for (int i = 0; i < totalFileCount; i++)
			PostThreadTask(i % g_threadCount, i, THREAD_TASK_READ_CALL);
		for (int i = 0; i < totalFileCount; i++)
			PostThreadTask(i % g_threadCount, i, THREAD_TASK_COMPLETION);
		for (int i = 0; i < totalFileCount; i++)
			PostThreadTask(i % g_threadCount, i, THREAD_TASK_COMPUTE);
	}
	delete s;

	// Send exit code to threads.
	for (int t = 0; t < g_threadCount; t++)
		PostThreadExit(t);

	// Wait for thread termination.
	WaitForMultipleObjects(g_threadCount, g_threadHandleAry, TRUE, INFINITE);

	// [VARIABLE] Release cached datas.
	{
		for (int i = 0; i < totalFileCount; i++)
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