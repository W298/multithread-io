#include "pch.h"
#include "ThreadSchedule.h"
#include "FileGenerator.h"

#define SERIES_INIT(name) \
	marker_series series(name);
#define SPAN_INIT \
	span* s = nullptr;
#define SPAN_START(cat, name) \
	s = new span(series, cat, name);
#define SPAN_END delete s;

#define GET_TASK_NAME(type) type == 0 ? "Read Call" : type == 1 ? "Completion" : "Compute"

using namespace Concurrency::diagnostic;
using namespace ThreadSchedule;

// Thread works.
HANDLE g_iocp;
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

DWORD ThreadSchedule::GetAlignedByteSize(PLARGE_INTEGER fileByteSize, DWORD sectorSize)
{
	return ((fileByteSize->QuadPart / sectorSize) + 1u) * sectorSize;
}

void ThreadSchedule::ReadCallTaskWork(UINT fid, marker_series& series)
{
	SPAN_INIT;

	SPAN_START(0, _T("Create File"));
	HANDLE fileHandle = CreateFileW((L"dummy\\" + std::to_wstring(fid)).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, g_fileFlag, NULL);
	SPAN_END;

	SPAN_START(0, _T("Create IOCP Handle"));
	
	// #Variable
	HANDLE fileIOCP = NULL;
	if (simType == SIM_A)
	{
		fileIOCP = CreateIoCompletionPort(fileHandle, NULL, 0, 0);
	}
	else if (simType == SIM_B)
	{
		CreateIoCompletionPort(fileHandle, g_iocp, fid, 0);
	}
	
	SPAN_END;

	SPAN_START(0, _T("Get File Size"));
	LARGE_INTEGER fileByteSize;
	DWORD alignedFileByteSize;

	GetFileSizeEx(fileHandle, &fileByteSize);
	alignedFileByteSize = GetAlignedByteSize(&fileByteSize, 512u);
	SPAN_END;

	SPAN_START(0, _T("Buffer Allocation"));
	BYTE* fileBuffer = (BYTE*)VirtualAlloc(NULL, alignedFileByteSize, MEM_COMMIT, PAGE_READWRITE);
	SPAN_END;

	SPAN_START(0, _T("Write to Map"));
	AcquireSRWLockExclusive(&g_srwFileHandle);
	{
		if (g_fileHandleMap.find(fid) == g_fileHandleMap.end())
			g_fileHandleMap[fid] = fileHandle;
	}
	ReleaseSRWLockExclusive(&g_srwFileHandle);
	
	// #Variable
	if (simType == SIM_A)
	{
		AcquireSRWLockExclusive(&g_srwFileIocp);
		{
			if (g_fileIocpMap.find(fid) == g_fileIocpMap.end())
				g_fileIocpMap[fid] = fileIOCP;
		}
		ReleaseSRWLockExclusive(&g_srwFileIocp);
	}
	else if (simType == SIM_B)
	{
		// Don't need to set IOCP map.
	}

	AcquireSRWLockExclusive(&g_srwFileBuffer);
	{
		if (g_fileBufferMap.find(fid) == g_fileBufferMap.end())
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

void ThreadSchedule::CompletionTaskWork(UINT fid, marker_series& series)
{
	SPAN_INIT;

	SPAN_START(1, _T("Ready"));
	DWORD fRet;
	ULONG_PTR fKey;
	LPOVERLAPPED fLpov;

	HANDLE fileIocp = NULL;
	AcquireSRWLockShared(&g_srwFileIocp);
	{
		fileIocp = g_fileIocpMap[fid];
	}
	ReleaseSRWLockShared(&g_srwFileIocp);
	SPAN_END;

	SPAN_START(1, _T("Waiting..."));
	GetQueuedCompletionStatus(fileIocp, &fRet, &fKey, &fLpov, INFINITE);
	SPAN_END;
}

void ThreadSchedule::ComputeTaskWork(UINT fid, marker_series& series)
{
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
				elapsedMicroSeconds = ((float)(end.QuadPart - start.QuadPart) / freq.QuadPart) * 1000.0f * 1000.0f;
				if (elapsedMicroSeconds > timeOverMicroSeconds)
				{
					SPAN_END;
					return;
				}

				if (num % i == 0)
					break;
				i++;
			}
			if (i == num)
				primes++;
		}
	}
	SPAN_END;
}

// #Variable
// Failed to get lock, which means another thread is processing task.
DWORD ThreadSchedule::HandleLockAcquireFailure(UINT fid, UINT threadTaskType, marker_series* workerSeries)
{
	UINT fileStatus = 0;

	HANDLE* fileSemAry = g_fileLockMap[fid]->sem;
	HANDLE* taskEndEvAry = g_fileLockMap[fid]->taskEndEv;
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
			span* s = new span(*workerSeries, 8, (L"Waiting Task " + std::to_wstring(threadTaskType - 1) + L" (FID " + std::to_wstring(fid) + L") - Pre require").c_str());
			DWORD result = WaitForSingleObject(fileSemAry[threadTaskType], INFINITE);
			delete s;

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
			span* s = new span(*workerSeries, 9, (L"Waiting Task " + std::to_wstring(threadTaskType) + L" (FID " + std::to_wstring(fid) + L")").c_str());
			WaitForSingleObject(taskEndEvAry[threadTaskType], INFINITE);
			delete s;
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

void ThreadSchedule::DoThreadTask(ThreadTaskArgs* args, UINT threadTaskType, marker_series* workerSeries)
{
	UINT fid = args->FID;

	HANDLE* fileSemAry = NULL;
	HANDLE* taskEndEvAry = NULL;

	// #Variable
	if (simType == SIM_A)
	{
		fileSemAry = g_fileLockMap[fid]->sem;
		taskEndEvAry = g_fileLockMap[fid]->taskEndEv;

		DWORD waitResult = WaitForSingleObject(fileSemAry[threadTaskType], 0L);
		if (waitResult == WAIT_TIMEOUT)	// If failed to get lock...
		{
			waitResult = HandleLockAcquireFailure(fid, threadTaskType, workerSeries);
			if (waitResult == WAIT_TIMEOUT)
			{
				HeapFree(GetProcessHeap(), 0, args);
				return;
			}
		}

		AcquireSRWLockExclusive(&g_srwFileStatus);
		InterlockedIncrement(&g_fileStatusMap[fid]);
		ReleaseSRWLockExclusive(&g_srwFileStatus);
	}
	else if (simType == SIM_B)
	{
		// Don't need to lock.
	}

	span* ss = nullptr;
	switch (threadTaskType)
	{
	case THREAD_TASK_READ_CALL:
		ss = new span(*workerSeries, threadTaskType, (L"Read Call Task (" + std::to_wstring(fid) + L")").c_str());
		break;
	case THREAD_TASK_COMPLETION:
		ss = new span(*workerSeries, threadTaskType, (L"Completion Task (" + std::to_wstring(fid) + L")").c_str());
		break;
	case THREAD_TASK_COMPUTE:
		ss = new span(*workerSeries, threadTaskType, (L"Compute Task (" + std::to_wstring(fid) + L")").c_str());
		break;
	}

	SERIES_INIT((std::to_wstring(GetCurrentThreadId()) + L"-Task Series").c_str());
	switch (threadTaskType)
	{
	case THREAD_TASK_READ_CALL:
		ReadCallTaskWork(fid, series);
		break;
	case THREAD_TASK_COMPLETION:
		CompletionTaskWork(fid, series);
		break;
	case THREAD_TASK_COMPUTE:
		ComputeTaskWork(fid, series);
		break;
	}
	delete ss;

	// #Variable
	if (simType == SIM_A)
	{
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
	}
	else if (simType == SIM_B)
	{
		// Don't need to lock.
	}

	HeapFree(GetProcessHeap(), 0, args);
}

DWORD WINAPI ThreadSchedule::ThreadFunc(LPVOID param)
{
	marker_series workerSeries(std::to_wstring(GetCurrentThreadId()).c_str());
	
	const HANDLE threadIOCPHandle = *(HANDLE*)(param);

	OVERLAPPED_ENTRY ent[g_taskRemoveCount];
	ULONG entRemoved;

	while (TRUE)
	{
		workerSeries.write_flag(1, _T("Waiting Task..."));

		GetQueuedCompletionStatusEx(threadIOCPHandle, ent, g_taskRemoveCount, &entRemoved, INFINITE, FALSE);

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

void ThreadSchedule::InsertThreadTaskFront(UINT t, UINT* fidAry, UINT* threadTaskTypeAry, UINT count)
{
	OVERLAPPED_ENTRY ent[1000];
	ULONG entRemoved;

	GetQueuedCompletionStatusEx(g_threadIocpAry[t], ent, 1000, &entRemoved, 0L, FALSE);

	for (int i = 0; i < count; i++)
		PostThreadTask(t, fidAry[i], threadTaskTypeAry[i]);

	for (int i = 0; i < entRemoved; i++)
	{
		if (ent[i].lpCompletionKey == g_exitCode)
		{
			PostThreadExit(t);
			continue;
		}
		
		ThreadTaskArgs* args = (ThreadTaskArgs*)(ent[i].lpOverlapped);
		PostThreadTask(t, args->FID, ent[i].lpCompletionKey);
	}
}

void ThreadSchedule::StartThreadTasks(UINT* rootFIDAry, UINT rootFIDAryCount)
{
	SERIES_INIT(_T("Main Thread - ThreadSchedule"));

	InitializeSRWLock(&g_srwFileLock);
	InitializeSRWLock(&g_srwFileHandle);
	InitializeSRWLock(&g_srwFileIocp);
	InitializeSRWLock(&g_srwFileBuffer);

	// #Variable Initialize global IOCP.
	if (simType == SIM_B)
		g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);

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

	// #Variable Create root file locking & status objects.
	if (simType == SIM_A)
	{
		for (int i = 0; i < rootFIDAryCount; i++)
		{
			const UINT rootFID = rootFIDAry[i];
			g_fileLockMap[rootFID] = new FileLock(rootFID);
			g_fileStatusMap[rootFID] = 0;
		}
	}
	else if (simType == SIM_B)
	{
		// Don't need to create lock object.
	}

	SPAN_INIT;

	// #Variable Post tasks.
	SPAN_START(3, _T("Send Tasks"));
	if (simType == SIM_A)
	{
		// 1. Uniformly distribute.
		/*for (int i = 0; i < rootFIDAryCount; i++)
		{
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_READ_CALL);
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_COMPLETION);
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_COMPUTE);
		}*/

		// 2. Define thread tasks.
		for (int i = 0; i < rootFIDAryCount/4; i++)
		{
			PostThreadTask(0, rootFIDAry[i*4], THREAD_TASK_READ_CALL);
			PostThreadTask(1, rootFIDAry[i*4+1], THREAD_TASK_READ_CALL);
			PostThreadTask(2, rootFIDAry[i*4+2], THREAD_TASK_READ_CALL);
			PostThreadTask(3, rootFIDAry[i*4+3], THREAD_TASK_READ_CALL);
		}

		for (int i = 0; i < rootFIDAryCount/2; i++)
		{
			PostThreadTask(4, rootFIDAry[i*2], THREAD_TASK_COMPLETION);
			PostThreadTask(5, rootFIDAry[i*2+1], THREAD_TASK_COMPLETION);
		}

		for (int i = 0; i < rootFIDAryCount/8; i++)
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
	else if (simType == SIM_B)
	{
		for (int i = 0; i < rootFIDAryCount / 4; i++)
		{
			PostThreadTask(0, rootFIDAry[i * 4], THREAD_TASK_READ_CALL);
			PostThreadTask(1, rootFIDAry[i * 4 + 1], THREAD_TASK_READ_CALL);
			PostThreadTask(2, rootFIDAry[i * 4 + 2], THREAD_TASK_READ_CALL);
			PostThreadTask(3, rootFIDAry[i * 4 + 3], THREAD_TASK_READ_CALL);
		}
	}
	SPAN_END;

	// #Variable Main Thread checks file read completion. 
	if (simType == SIM_B)
	{
		LPOVERLAPPED_ENTRY ent = new OVERLAPPED_ENTRY[rootFIDAryCount];
		ULONG realRemovedCount;
		ULONG desireCount = rootFIDAryCount;

		UINT threadToWork = 0;
		while (desireCount > 0)
		{
			SPAN_START(4, _T("Waiting Completion..."));
			GetQueuedCompletionStatusEx(g_iocp, ent, rootFIDAryCount, &realRemovedCount, INFINITE, FALSE);
			SPAN_END;

			SPAN_START(5, _T("Handle Completion"));
			for (int i = 0; i < realRemovedCount; i++)
			{
				ULONG_PTR key = ent[i].lpCompletionKey;
				LPOVERLAPPED lpov = ent[i].lpOverlapped;

				PostThreadTask(4 + threadToWork, key, THREAD_TASK_COMPUTE);
				threadToWork = (threadToWork + 1) % 10;
			}

			desireCount -= realRemovedCount;
			SPAN_END;
		}

		delete[] ent;
	}

	// Send exit code to threads.
	for (int t = 0; t < g_threadCount; t++)
		PostThreadExit(t);

	// Wait for thread termination.
	WaitForMultipleObjects(g_threadCount, g_threadHandleAry, TRUE, INFINITE);

	// #Variable Release cached data.
	{
		for (int i = 0; i < g_fileBufferMap.size(); i++)
			VirtualFree(g_fileBufferMap[i], 0, MEM_RELEASE);

		for (int i = 0; i < g_fileIocpMap.size(); i++)
			CloseHandle(g_fileIocpMap[i]);

		for (int i = 0; i < g_fileHandleMap.size(); i++)
			CloseHandle(g_fileHandleMap[i]);
		
		for (int i = 0; i < g_fileLockMap.size(); i++)
			delete g_fileLockMap[i];

		g_fileBufferMap.clear();
		g_fileIocpMap.clear();
		g_fileHandleMap.clear();
		g_fileLockMap.clear();
	}

	// #Variable Close global IOCP.
	if (simType == SIM_B)
		CloseHandle(g_iocp);

	// Release thread handle/IOCP.
	for (int t = 0; t < g_threadCount; t++)
	{
		CloseHandle(g_threadIocpAry[t]);
		CloseHandle(g_threadHandleAry[t]);
	}
}