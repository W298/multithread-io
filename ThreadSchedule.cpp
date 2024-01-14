#include "pch.h"
#include "ThreadSchedule.h"

using namespace Concurrency::diagnostic;
using namespace ThreadSchedule;

// Thread works.
HANDLE g_globalTaskQueue;
HANDLE g_globalWaitingQueue;
HANDLE g_globalFinishQueue;
HANDLE g_threadHandleAry[g_threadCount];
HANDLE g_threadIocpAry[g_threadCount];

SRWLOCK g_srwFileStatus;
SRWLOCK g_srwFileLock;
SRWLOCK g_srwFileHandle;
SRWLOCK g_srwFileIocp;
SRWLOCK g_srwFileBuffer;

// File cache data.
std::unordered_map<UINT, UINT> g_fileStatusMap;
std::unordered_map<UINT, FileLock*> g_fileLockMap;
std::unordered_map<UINT, HANDLE> g_fileHandleMap;
std::unordered_map<UINT, HANDLE> g_fileIocpMap;
std::unordered_map<UINT, BYTE*> g_fileBufferMap;

void ThreadSchedule::ReadCallTaskWork(const UINT fid)
{
	SERIES_INIT((std::to_wstring(GetCurrentThreadId()) + L" Read Call Task").c_str());
	SPAN_INIT;

	SPAN_START(0, _T("Create File"));
	const HANDLE fileHandle = 
		CreateFileW(
			(L"dummy\\" + std::to_wstring(fid)).c_str(), 
			GENERIC_READ, 
			FILE_SHARE_READ, 
			NULL, 
			OPEN_EXISTING, 
			g_fileFlag, 
			NULL);
	SPAN_END;

	// #Variable
	SPAN_START(0, _T("Create IOCP Handle"));
	HANDLE fileIOCP = NULL;
	if (g_simType == SIM_UNIVERSAL_THREAD)
	{
		fileIOCP = CreateIoCompletionPort(fileHandle, NULL, 0, 0);
	}
	else if (g_simType == SIM_ROLE_SPECIFIED_THREAD)
	{
		// All IOCP is same, we don't need to save.
		CreateIoCompletionPort(fileHandle, g_globalWaitingQueue, fid, 0);
	}
	SPAN_END;

	SPAN_START(0, _T("Get File Size"));
	LARGE_INTEGER fileByteSize;
	GetFileSizeEx(fileHandle, &fileByteSize);

	const DWORD alignedFileByteSize = GetAlignedByteSize(&fileByteSize, 512u);
	SPAN_END;

	SPAN_START(0, _T("Buffer Allocation"));
	BYTE* fileBuffer = static_cast<BYTE*>(VirtualAlloc(NULL, alignedFileByteSize, MEM_COMMIT, PAGE_READWRITE));
	SPAN_END;

	SPAN_START(0, _T("Write to Map"));
	AcquireSRWLockExclusive(&g_srwFileHandle);
	{
		if (FALSE == g_fileHandleMap.contains(fid))
			g_fileHandleMap[fid] = fileHandle;
	}
	ReleaseSRWLockExclusive(&g_srwFileHandle);
	
	// #Variable
	if (g_simType == SIM_UNIVERSAL_THREAD)
	{
		AcquireSRWLockExclusive(&g_srwFileIocp);
		{
			if (FALSE == g_fileIocpMap.contains(fid))
				g_fileIocpMap[fid] = fileIOCP;
		}
		ReleaseSRWLockExclusive(&g_srwFileIocp);
	}
	else if (g_simType == SIM_ROLE_SPECIFIED_THREAD)
	{
		// Don't need to set IOCP map.
	}

	AcquireSRWLockExclusive(&g_srwFileBuffer);
	{
		if (FALSE == g_fileBufferMap.contains(fid))
			g_fileBufferMap[fid] = fileBuffer;
	}
	ReleaseSRWLockExclusive(&g_srwFileBuffer);
	SPAN_END;

	SPAN_START(0, _T("ReadFile Call"));
	OVERLAPPED ov = { 0 };
	if (ReadFile(fileHandle, fileBuffer, alignedFileByteSize, NULL, &ov) == FALSE && GetLastError() != ERROR_IO_PENDING)
		ExitProcess(-1);
	SPAN_END;
}

void ThreadSchedule::CompletionTaskWork(const UINT fid)
{
	SERIES_INIT((std::to_wstring(GetCurrentThreadId()) + L" Completion Task").c_str());
	SPAN_INIT;

	SPAN_START(1, _T("Ready"));
	HANDLE fileIocp = NULL;
	AcquireSRWLockShared(&g_srwFileIocp);
	{
		fileIocp = g_fileIocpMap[fid];
	}
	ReleaseSRWLockShared(&g_srwFileIocp);

	DWORD ret;
	ULONG_PTR key;
	LPOVERLAPPED lpov;
	SPAN_END;

	SPAN_START(1, _T("Waiting..."));
	GetQueuedCompletionStatus(fileIocp, &ret, &key, &lpov, INFINITE);
	SPAN_END;
}

void ThreadSchedule::ComputeTaskWork(const UINT fid)
{
	SERIES_INIT((std::to_wstring(GetCurrentThreadId()) + L" Compute Task").c_str());
	SPAN_INIT;

	SPAN_START(2, _T("Ready Paramters"));
	LARGE_INTEGER freq, start, end;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);

	BYTE* bufferAddress = nullptr;
	AcquireSRWLockShared(&g_srwFileBuffer);
	{
		bufferAddress = g_fileBufferMap[fid];
	}
	ReleaseSRWLockShared(&g_srwFileBuffer);

	UINT timeOverMicroSeconds;
	memcpy(&timeOverMicroSeconds, bufferAddress, sizeof(UINT));
	SPAN_END;

	SPAN_START(2, _T("Start Compute"));
	float elapsedMicroSeconds = 0.0f;
	while (elapsedMicroSeconds <= timeOverMicroSeconds)
	{
		int num = 1, primes = 0;
		for (num = 1; num <= 5; num++) 
		{
			int i = 2;
			while (i <= num) 
			{
				QueryPerformanceCounter(&end);
				elapsedMicroSeconds = (
					static_cast<float>(end.QuadPart - start.QuadPart) / 
					static_cast<float>(freq.QuadPart)) * 1000.0f * 1000.0f;

				if (elapsedMicroSeconds > timeOverMicroSeconds)
					goto FINALIZE;

				if (num % i == 0)
					break;
				i++;
			}
			if (i == num)
				primes++;
		}
	}

FINALIZE:
	SPAN_END;
	OVERLAPPED ov = { 0 };
	PostQueuedCompletionStatus(g_globalFinishQueue, 0, fid, &ov);
}

// #Variable
// Failed to get lock, which means another thread is processing task.
DWORD ThreadSchedule::HandleLockAcquireFailure(const UINT fid, const UINT threadTaskType, const marker_series& series)
{
	SPAN_INIT;

	const HANDLE* fileSemAry = g_fileLockMap[fid]->sem;
	const HANDLE* taskEndEvAry = g_fileLockMap[fid]->taskEndEv;

	UINT fileStatus = 0;
	AcquireSRWLockShared(&g_srwFileStatus);
	{
		fileStatus = g_fileStatusMap[fid];
	}
	ReleaseSRWLockShared(&g_srwFileStatus);

	// Another thread is processing pre-require task.
	if (fileStatus < threadTaskType * 3)
	{
		if (g_waitDependencyFront)
		{
			// Waiting until pre-require task ends.
			const LPCTSTR format = L"Waiting Task %d (FID %d) - Pre require";

			SPAN_START(8, format, threadTaskType, fid);
			const DWORD result = WaitForSingleObject(fileSemAry[threadTaskType], INFINITE);
			SPAN_END;

			// Do your job!
			return result;
		}
		// If g_waitDependencyFront false, we'll check later.
		
		// Ignore your job!
		return WAIT_TIMEOUT;
	}

	// Another thread is processing requested task.
	if (threadTaskType * 3 <= fileStatus && fileStatus < (threadTaskType + 1) * 3)
	{
		if (g_waitDependencyFront)
		{
			// Waiting until requested task ends.
			const LPCTSTR format = L"Waiting Task %d (FID %d)";

			SPAN_START(9, format, threadTaskType, fid);
			WaitForSingleObject(taskEndEvAry[threadTaskType], INFINITE);
			SPAN_END;
		}
		// If g_waitDependencyFront false, we'll check later.

		// Ignore your job!
		return WAIT_TIMEOUT;
	}

	// Another thread is processing post-require task.
	if ((threadTaskType + 1) * 3 <= fileStatus)
	{
		// Ignore your job!
		return WAIT_TIMEOUT;
	}

	return WAIT_TIMEOUT;
}

void ThreadSchedule::DoThreadTask(ThreadTaskArgs* args, const UINT threadTaskType, const marker_series& series)
{
	SPAN_INIT;

	const UINT fid = args->FID;
	const HANDLE* fileSemAry = g_fileLockMap[fid]->sem;
	const HANDLE* taskEndEvAry = g_fileLockMap[fid]->taskEndEv;

	DWORD waitResult = WaitForSingleObject(fileSemAry[threadTaskType], 0L);
	if (waitResult == WAIT_TIMEOUT)	// If failed to get lock...
	{
		waitResult = HandleLockAcquireFailure(fid, threadTaskType, series);
		if (waitResult == WAIT_TIMEOUT)
		{
			HeapFree(GetProcessHeap(), 0, args);
			return;
		}
	}

	AcquireSRWLockExclusive(&g_srwFileStatus);
	InterlockedIncrement(&g_fileStatusMap[fid]);
	ReleaseSRWLockExclusive(&g_srwFileStatus);

	const LPCTSTR format =
		threadTaskType == THREAD_TASK_READ_CALL ? L"Read Call Task (%d)" :
		threadTaskType == THREAD_TASK_COMPLETION ? L"Completion Task (%d)" :
		L"Compute Task (%d)";
	
	SPAN_START(threadTaskType, format, fid);
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
	SPAN_END;

	AcquireSRWLockExclusive(&g_srwFileStatus);
	InterlockedIncrement(&g_fileStatusMap[fid]);
	ReleaseSRWLockExclusive(&g_srwFileStatus);

	if (threadTaskType < THREAD_TASK_COMPUTE)
	{
		ReleaseSemaphore(fileSemAry[threadTaskType + 1], 1, NULL);

		AcquireSRWLockExclusive(&g_srwFileStatus);
		InterlockedIncrement(&g_fileStatusMap[fid]);
		ReleaseSRWLockExclusive(&g_srwFileStatus);
	}

	SetEvent(taskEndEvAry[threadTaskType]);

	HeapFree(GetProcessHeap(), 0, args);
}

DWORD WINAPI ThreadSchedule::UniversalThreadFunc(const LPVOID param)
{
	const marker_series workerSeries(std::to_wstring(GetCurrentThreadId()).c_str());
	const HANDLE threadIOCPHandle = param;

	OVERLAPPED_ENTRY entryAry[g_taskRemoveCount];
	ULONG entRemoved;

	while (TRUE)
	{
		GetQueuedCompletionStatusEx(threadIOCPHandle, entryAry, g_taskRemoveCount, &entRemoved, INFINITE, FALSE);

		for (UINT i = 0; i < entRemoved; i++)
		{
			const ULONG_PTR key = entryAry[i].lpCompletionKey;
			const LPOVERLAPPED lpov = entryAry[i].lpOverlapped;

			// If exit code received, terminate thread.
			if (key == g_exitCode)
				return 0;

			ThreadTaskArgs* args = reinterpret_cast<ThreadTaskArgs*>(lpov);
			DoThreadTask(args, static_cast<UINT>(key), workerSeries);
		}
	}

	return 0;
}

DWORD ThreadSchedule::RoleSpecifiedThreadFunc(const LPVOID param)
{
	SERIES_INIT(std::to_wstring(GetCurrentThreadId()).c_str());
	SPAN_INIT;

	const UINT_PTR threadRole = reinterpret_cast<UINT_PTR>(param);

	DWORD ret;
	ULONG_PTR key;
	LPOVERLAPPED lpov;

	if (threadRole == THREAD_TYPE_CALL_ONLY)
	{
		while (TRUE)
		{
			GetQueuedCompletionStatus(g_globalTaskQueue, &ret, &key, &lpov, INFINITE);

			SPAN_START(0, _T("Read Call Task"));
			ReadCallTaskWork(static_cast<UINT>(key));
			SPAN_END;
		}
	}
	else if (threadRole == THREAD_TYPE_COMPUTE_ONLY)
	{
		while (TRUE)
		{
			GetQueuedCompletionStatus(g_globalWaitingQueue, &ret, &key, &lpov, INFINITE);

			SPAN_START(2, _T("Compute Task"));
			ComputeTaskWork(static_cast<UINT>(key));
			SPAN_END;
		}
	}

	return 0;
}

void ThreadSchedule::StartThreadTasks(const UINT* rootFIDAry, const UINT rootFIDAryCount)
{
	SERIES_INIT(_T("Main Thread"));
	SPAN_INIT;

	InitializeSRWLock(&g_srwFileLock);
	InitializeSRWLock(&g_srwFileHandle);
	InitializeSRWLock(&g_srwFileIocp);
	InitializeSRWLock(&g_srwFileBuffer);

	// #Variable Initialize global IOCP.
	if (g_simType == SIM_ROLE_SPECIFIED_THREAD)
	{
		g_globalTaskQueue = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);
		g_globalWaitingQueue = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);
		g_globalFinishQueue = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);
	}

	// #Variable Create threads.
	for (UINT t = 0; t < g_threadCount; t++)
	{
		DWORD tid = 0;
		HANDLE threadHandle = NULL;

		if (g_simType == SIM_UNIVERSAL_THREAD)
		{
			g_threadIocpAry[t] = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
			threadHandle = CreateThread(NULL, 0, UniversalThreadFunc, g_threadIocpAry[t], 0, &tid);
		}
		else if (g_simType == SIM_ROLE_SPECIFIED_THREAD)
		{
			const UINT_PTR threadType = t < 5 ? THREAD_TYPE_CALL_ONLY : THREAD_TYPE_COMPUTE_ONLY;
			threadHandle = CreateThread(NULL, 0, RoleSpecifiedThreadFunc, reinterpret_cast<LPVOID>(threadType), 0, &tid);
		}

		if (threadHandle == NULL)
			ExitProcess(3);

		g_threadHandleAry[t] = threadHandle;
	}

	// #Variable Create root file locking & status objects.
	if (g_simType == SIM_UNIVERSAL_THREAD)
	{
		for (UINT i = 0; i < rootFIDAryCount; i++)
		{
			const UINT rootFID = rootFIDAry[i];
			g_fileLockMap[rootFID] = new FileLock(rootFID);
			g_fileStatusMap[rootFID] = 0;
		}
	}
	else if (g_simType == SIM_ROLE_SPECIFIED_THREAD)
	{
		// Don't need to create lock object.
	}

	// #Variable Post tasks.
	SPAN_START(3, _T("Send Tasks"));
	if (g_simType == SIM_UNIVERSAL_THREAD)
	{
		// 1. Uniformly distribute.
		/*for (UINT i = 0; i < rootFIDAryCount; i++)
		{
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_READ_CALL);
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_COMPLETION);
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_COMPUTE);
		}*/

		// 2. Define thread tasks.
		for (UINT i = 0; i < rootFIDAryCount/4; i++)
		{
			PostThreadTask(0, rootFIDAry[i*4], THREAD_TASK_READ_CALL);
			PostThreadTask(1, rootFIDAry[i*4+1], THREAD_TASK_READ_CALL);
			PostThreadTask(2, rootFIDAry[i*4+2], THREAD_TASK_READ_CALL);
			PostThreadTask(3, rootFIDAry[i*4+3], THREAD_TASK_READ_CALL);
		}

		for (UINT i = 0; i < rootFIDAryCount/2; i++)
		{
			PostThreadTask(4, rootFIDAry[i*2], THREAD_TASK_COMPLETION);
			PostThreadTask(5, rootFIDAry[i*2+1], THREAD_TASK_COMPLETION);
		}

		for (UINT i = 0; i < rootFIDAryCount/8; i++)
		{
			PostThreadTask(6, rootFIDAry[i*8], THREAD_TASK_COMPUTE);
			PostThreadTask(7, rootFIDAry[i*8+1], THREAD_TASK_COMPUTE);
			PostThreadTask(8, rootFIDAry[i*8+2], THREAD_TASK_COMPUTE);
			PostThreadTask(9, rootFIDAry[i*8+3], THREAD_TASK_COMPUTE);
			PostThreadTask(10, rootFIDAry[i*8+4], THREAD_TASK_COMPUTE);
			PostThreadTask(11, rootFIDAry[i*8+5], THREAD_TASK_COMPUTE);
			PostThreadTask(12, rootFIDAry[i*8+6], THREAD_TASK_COMPUTE);
			PostThreadTask(13, rootFIDAry[i*8+7], THREAD_TASK_COMPUTE);
		}
	}
	else if (g_simType == SIM_ROLE_SPECIFIED_THREAD)
	{
		for (UINT i = 0; i < rootFIDAryCount; i++)
		{
			OVERLAPPED ov = { 0 };
			PostQueuedCompletionStatus(g_globalTaskQueue, 0, rootFIDAry[i], &ov);
		}
	}
	SPAN_END;

	// #Variable Wait for thread termination.
	if (g_simType == SIM_UNIVERSAL_THREAD)
	{
		for (UINT t = 0; t < g_threadCount; t++)
			PostThreadExit(t);

		WaitForMultipleObjects(g_threadCount, g_threadHandleAry, TRUE, INFINITE);
	}
	else if (g_simType == SIM_ROLE_SPECIFIED_THREAD)
	{
		UINT finishedFileCount = 0;

		LPOVERLAPPED_ENTRY entAry = new OVERLAPPED_ENTRY[rootFIDAryCount];
		ULONG entRemoved;

		while (finishedFileCount < rootFIDAryCount)
		{
			GetQueuedCompletionStatusEx(g_globalFinishQueue, entAry, rootFIDAryCount, &entRemoved, 0L, FALSE);
			finishedFileCount += entRemoved;
		}

		for (UINT t = 0; t < g_threadCount; t++)
			TerminateThread(g_threadHandleAry[t], 100);
	}

	// #Variable Release cached data.
	{
		for (UINT i = 0; i < g_fileBufferMap.size(); i++)
			VirtualFree(g_fileBufferMap[i], 0, MEM_RELEASE);

		for (UINT i = 0; i < g_fileIocpMap.size(); i++)
			CloseHandle(g_fileIocpMap[i]);

		for (UINT i = 0; i < g_fileHandleMap.size(); i++)
			CloseHandle(g_fileHandleMap[i]);
		
		for (UINT i = 0; i < g_fileLockMap.size(); i++)
			delete g_fileLockMap[i];

		g_fileBufferMap.clear();
		g_fileIocpMap.clear();
		g_fileHandleMap.clear();
		g_fileLockMap.clear();
	}

	// #Variable Close global IOCP.
	if (g_simType == SIM_ROLE_SPECIFIED_THREAD)
	{
		CloseHandle(g_globalTaskQueue);
		CloseHandle(g_globalWaitingQueue);
		CloseHandle(g_globalFinishQueue);
	}

	// #Variable Release thread handle/IOCP.
	for (UINT t = 0; t < g_threadCount; t++)
	{
		if (g_simType == SIM_UNIVERSAL_THREAD)
			CloseHandle(g_threadIocpAry[t]);

		CloseHandle(g_threadHandleAry[t]);
	}
}

DWORD ThreadSchedule::GetAlignedByteSize(const PLARGE_INTEGER fileByteSize, const DWORD sectorSize)
{
	return ((fileByteSize->QuadPart / sectorSize) + 1u) * sectorSize;
}

void ThreadSchedule::PostThreadTask(const UINT t, const UINT fid, const UINT threadTaskType)
{
	ThreadTaskArgs* args = static_cast<ThreadTaskArgs*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThreadTaskArgs)));
	args->FID = fid;

	if (FALSE == PostQueuedCompletionStatus(g_threadIocpAry[t], 0, threadTaskType, reinterpret_cast<LPOVERLAPPED>(args)))
		ExitProcess(9);
}

void ThreadSchedule::PostThreadExit(const UINT t)
{
	if (FALSE == PostQueuedCompletionStatus(g_threadIocpAry[t], 0, g_exitCode, NULL))
		ExitProcess(9);
}

void ThreadSchedule::InsertThreadTaskFront(const UINT t, const UINT* fidAry, const UINT* threadTaskTypeAry, const UINT count)
{
	OVERLAPPED_ENTRY entAry[1000];
	ULONG entRemoved;

	GetQueuedCompletionStatusEx(g_threadIocpAry[t], entAry, 1000, &entRemoved, 0L, FALSE);

	for (UINT i = 0; i < count; i++)
		PostThreadTask(t, fidAry[i], threadTaskTypeAry[i]);

	for (UINT i = 0; i < entRemoved; i++)
	{
		if (entAry[i].lpCompletionKey == g_exitCode)
		{
			PostThreadExit(t);
			continue;
		}

		const ThreadTaskArgs* args = reinterpret_cast<ThreadTaskArgs*>(entAry[i].lpOverlapped);
		PostThreadTask(t, args->FID, static_cast<UINT>(entAry[i].lpCompletionKey));
	}
}