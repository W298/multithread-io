#include "pch.h"
#include "ThreadSchedule.h"
#include "FileGenerator.h"

using namespace Concurrency::diagnostic;
using namespace ThreadSchedule;

// Thread works.
HANDLE g_iocp;
HANDLE g_threadHandleAry[g_threadCount];
HANDLE g_threadIocpAry[g_threadCount];
std::unordered_map<DWORD, UINT> g_tidMap;

SRWLOCK g_srwFileLock;
SRWLOCK g_srwFileHandle;
SRWLOCK g_srwFileIocp;
SRWLOCK g_srwFileBuffer;
SRWLOCK g_srwFileDep;

// File cache data.
std::unordered_map<UINT, FileLock*> g_fileLockMap;
std::unordered_map<UINT, HANDLE> g_fileHandleMap;
std::unordered_map<UINT, HANDLE> g_fileIocpMap;
std::unordered_map<UINT, BYTE*> g_fileBufferMap;
std::unordered_map<UINT, std::vector<std::pair<UINT, BYTE>>> g_fileDepMap;

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

	AcquireSRWLockExclusive(&g_srwFileHandle);
	{
		if (g_fileHandleMap.find(fid) == g_fileHandleMap.end())
			g_fileHandleMap[fid] = fileHandle;
	}
	ReleaseSRWLockExclusive(&g_srwFileHandle);

	AcquireSRWLockExclusive(&g_srwFileIocp);
	{
		if (g_fileIocpMap.find(fid) == g_fileIocpMap.end())
			g_fileIocpMap[fid] = fileIOCP;
	}
	ReleaseSRWLockExclusive(&g_srwFileIocp);

	AcquireSRWLockExclusive(&g_srwFileBuffer);
	{
		if (g_fileBufferMap.find(fid) == g_fileBufferMap.end())
			g_fileBufferMap[fid] = fileBuffer;
	}
	ReleaseSRWLockExclusive(&g_srwFileBuffer);

	OVERLAPPED ov = { 0 };
	if (ReadFile(fileHandle, fileBuffer, alignedFileByteSize, NULL, &ov) == FALSE && GetLastError() != ERROR_IO_PENDING)
		ExitProcess(-1);
}

void ThreadSchedule::CompletionTaskWork(UINT fid)
{
	DWORD fRet;
	ULONG_PTR fKey;
	LPOVERLAPPED fLpov;

	HANDLE fileIocp = fileIocp = g_fileIocpMap[fid];
	GetQueuedCompletionStatus(fileIocp, &fRet, &fKey, &fLpov, INFINITE);
}

void ThreadSchedule::DependencyTaskWork(UINT fid)
{
	BYTE* bufferAddress = nullptr;
	AcquireSRWLockShared(&g_srwFileBuffer);
	{
		bufferAddress = g_fileBufferMap[fid];
	}
	ReleaseSRWLockShared(&g_srwFileBuffer);

	BYTE* cursor = bufferAddress + sizeof(UINT);

	UINT dependencyFileCount;
	memcpy(&dependencyFileCount, cursor, sizeof(UINT));
	cursor += sizeof(UINT);

	std::vector<std::pair<UINT, BYTE>> depVec;

	for (int dep = 0; dep < dependencyFileCount; dep++)
	{
		UINT depFID;
		BYTE depType;

		memcpy(&depFID, cursor, sizeof(UINT));
		cursor += sizeof(UINT);

		memcpy(&depType, cursor, sizeof(BYTE));
		cursor += sizeof(BYTE);

		depVec.emplace_back(depFID, depType);
	}

	AcquireSRWLockExclusive(&g_srwFileDep);
	{
		g_fileDepMap[fid] = depVec;
	}
	ReleaseSRWLockExclusive(&g_srwFileDep);

	const UINT t = g_tidMap[GetCurrentThreadId()];

	AcquireSRWLockExclusive(&g_srwFileLock);
	{
		for (int dep = 0; dep < depVec.size(); dep++)
		{
			const UINT depFID = depVec[dep].first;
			if (g_fileLockMap.find(depFID) == g_fileLockMap.end())
				g_fileLockMap[depFID] = new FileLock(depFID);
		}
	}
	ReleaseSRWLockExclusive(&g_srwFileLock);

	std::vector<UINT> fidVec;
	std::vector<UINT> taskTypeVec;

	// [VARIABLE]
	for (int dep = 0; dep < depVec.size(); dep++)
	{
		const UINT depFID = depVec[dep].first;
		const UINT depType = depVec[dep].second;

		UINT depFileStatus = 0;
		AcquireSRWLockShared(&g_srwFileLock);
		{
			depFileStatus = g_fileLockMap[depFID]->status;
		}
		ReleaseSRWLockShared(&g_srwFileLock);

		// Just need raw data.
		if (depType == FileGenerator::FILE_DEPENDENCY_ONLY_READ)
		{
			if (depFileStatus >= FILE_STATUS_COMPLETION_TASK_COMPLETED)
			{
				// Raw data guaranteed.
				continue;
			}
			else
			{
				switch (depFileStatus)
				{
				case FILE_STATUS_READ_CALL_TASK_WAITING:
				{
					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_READ_CALL);

					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_COMPLETION);
					break;
				}
				case FILE_STATUS_READ_CALL_TASK_STARTED:
				case FILE_STATUS_READ_CALL_TASK_COMPLETED:
				case FILE_STATUS_COMPLETION_TASK_WAITING:
				{
					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_COMPLETION);
					break;
				}
				case FILE_STATUS_COMPLETION_TASK_STARTED:
					break;
				}
			}
		}
		// Need computed data.
		else 
		{
			if (depFileStatus >= FILE_STATUS_COMPUTE_TASK_COMPLETED)
			{
				// Computed data guaranteed. 
				continue;
			}
			else 
			{
				switch (depFileStatus)
				{
				case FILE_STATUS_READ_CALL_TASK_WAITING:
				{
					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_READ_CALL);

					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_COMPLETION);

					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_DEPENDENCY);

					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_COMPUTE);
					break;
				}
				case FILE_STATUS_READ_CALL_TASK_STARTED:
				case FILE_STATUS_READ_CALL_TASK_COMPLETED:
				case FILE_STATUS_COMPLETION_TASK_WAITING:
				{
					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_COMPLETION);

					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_DEPENDENCY);

					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_COMPUTE);
					break;
				}
				case FILE_STATUS_COMPLETION_TASK_STARTED:
				case FILE_STATUS_COMPLETION_TASK_COMPLETED:
				case FILE_STATUS_DEPENDENCY_TASK_WAITING:
				{
					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_DEPENDENCY);

					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_COMPUTE);
				}
				case FILE_STATUS_DEPENDENCY_TASK_STARTED:
				case FILE_STATUS_DEPENDENCY_TASK_COMPLETED:
				case FILE_STATUS_COMPUTE_TASK_WAITING:
				{
					fidVec.push_back(depFID);
					taskTypeVec.push_back(THREAD_TASK_COMPUTE);
				}
				case FILE_STATUS_COMPUTE_TASK_STARTED:
				case FILE_STATUS_COMPUTE_TASK_COMPLETED:
					break;
				}
			}
		}
	}

	InsertThreadTaskFront(t, fidVec.data(), taskTypeVec.data(), fidVec.size());
}

void ThreadSchedule::ComputeTaskWork(UINT fid)
{
	// Check all dependency is ok. If not, wait.
	if (g_waitDependencyFront == FALSE)
	{
		std::vector<HANDLE> waitEvHandleVec;
		
		std::vector<std::pair<UINT, BYTE>> depVec;
		AcquireSRWLockShared(&g_srwFileDep);
		{
			depVec = g_fileDepMap[fid];
		}
		ReleaseSRWLockShared(&g_srwFileDep);
		
		for (int dep = 0; dep < depVec.size(); dep++)
		{
			const UINT depFID = depVec[dep].first;
			const UINT depType = depVec[dep].second;

			UINT depFileStatus = 0;
			HANDLE depFileTaskEndEvCompletion = g_fileLockMap[depFID]->taskEndEv[THREAD_TASK_COMPLETION];
			HANDLE depFileTaskEndEvCompute = g_fileLockMap[depFID]->taskEndEv[THREAD_TASK_COMPUTE];
			AcquireSRWLockShared(&g_srwFileLock);
			{
				depFileStatus = g_fileLockMap[depFID]->status;
			}
			ReleaseSRWLockShared(&g_srwFileLock);

			if (depType == FileGenerator::FILE_DEPENDENCY_ONLY_READ)
			{
				if (depFileStatus < FILE_STATUS_COMPLETION_TASK_COMPLETED)
					waitEvHandleVec.push_back(depFileTaskEndEvCompletion);
			}
			else
			{
				if (depFileStatus < FILE_STATUS_COMPUTE_TASK_COMPLETED)
					waitEvHandleVec.push_back(depFileTaskEndEvCompute);
			}
		}

		WaitForMultipleObjects(waitEvHandleVec.size(), waitEvHandleVec.data(), TRUE, INFINITE);
	}

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

// [VARIABLE]
// Failed to get lock, which means another thread is processing task.
DWORD ThreadSchedule::HandleLockAcquireFailure(UINT fid, UINT threadTaskType, marker_series* workerSeries)
{
	UINT fileStatus = 0;
	HANDLE* fileSemAry = g_fileLockMap[fid]->sem;
	HANDLE* taskEndEvAry = g_fileLockMap[fid]->taskEndEv;
	AcquireSRWLockShared(&g_srwFileLock);
	{
		fileStatus = g_fileLockMap[fid]->status;
	}
	ReleaseSRWLockShared(&g_srwFileLock);

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

	HANDLE* fileSemAry = g_fileLockMap[fid]->sem;
	HANDLE* taskEndEvAry = g_fileLockMap[fid]->taskEndEv;

	// [VARIABLE]
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

	InterlockedIncrement(&g_fileLockMap[fid]->status);

	span* s = new span(*workerSeries, threadTaskType, (L"Task " + std::to_wstring(threadTaskType) + L" (FID " + std::to_wstring(fid) + L")").c_str());
	switch (threadTaskType)
	{
	case THREAD_TASK_READ_CALL:
		ReadCallTaskWork(fid);
		break;
	case THREAD_TASK_COMPLETION:
		CompletionTaskWork(fid);
		break;
	case THREAD_TASK_DEPENDENCY:
		DependencyTaskWork(fid);
		break;
	case THREAD_TASK_COMPUTE:
		ComputeTaskWork(fid);
		break;
	}
	delete s;

	InterlockedIncrement(&g_fileLockMap[fid]->status);

	if (threadTaskType < THREAD_TASK_COMPUTE)
	{
		ReleaseSemaphore(fileSemAry[threadTaskType + 1], 1, NULL);
		InterlockedIncrement(&g_fileLockMap[fid]->status);
	}

	SetEvent(taskEndEvAry[threadTaskType]);

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
	marker_series mainThreadSeries(_T("Main Thread - ThreadSchedule"));

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, g_threadCount);
	
	InitializeSRWLock(&g_srwFileLock);
	InitializeSRWLock(&g_srwFileHandle);
	InitializeSRWLock(&g_srwFileIocp);
	InitializeSRWLock(&g_srwFileBuffer);
	InitializeSRWLock(&g_srwFileDep);

	// Create thread handles.
	for (int t = 0; t < g_threadCount; t++)
	{
		g_threadIocpAry[t] = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);

		DWORD tid;
		const HANDLE tHandle = CreateThread(NULL, 0, ThreadFunc, &g_threadIocpAry[t], 0, &tid);
		if (tHandle == NULL)
			ExitProcess(3);

		g_tidMap[tid] = t;
		g_threadHandleAry[t] = tHandle;
	}

	// Create root file status objects.
	for (int i = 0; i < rootFIDAryCount; i++)
	{
		UINT rootFID = rootFIDAry[i];
		g_fileLockMap[rootFID] = new FileLock(rootFID);
	}

	// [VARIABLE] #Scenario. Main thread assign threads what to do.
	span* s = new span(mainThreadSeries, 1, _T("Send Tasks"));
	{
		for (int i = 0; i < rootFIDAryCount; i++)
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_READ_CALL);
		
		for (int i = 0; i < rootFIDAryCount; i++)
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_COMPLETION);
		
		for (int i = 0; i < rootFIDAryCount; i++)
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_DEPENDENCY);
		
		for (int i = 0; i < rootFIDAryCount; i++)
			PostThreadTask(i % g_threadCount, rootFIDAry[i], THREAD_TASK_COMPUTE);
	}
	delete s;

	// Send exit code to threads.
	for (int t = 0; t < g_threadCount; t++)
		PostThreadExit(t);

	// Wait for thread termination.
	WaitForMultipleObjects(g_threadCount, g_threadHandleAry, TRUE, INFINITE);

	// [VARIABLE] Release cached datas.
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

	// Release thread handle/iocp.
	for (int t = 0; t < g_threadCount; t++)
	{
		CloseHandle(g_threadIocpAry[t]);
		CloseHandle(g_threadHandleAry[t]);
	}
}