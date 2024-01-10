#pragma once

namespace ThreadSchedule
{
	enum ThreadType
	{
		THREAD_TYPE_GENERAL_NON_OVERLAP,
		THREAD_TYPE_GENERAL,
		THREAD_TYPE_CALL_ONLY,
		THREAD_TYPE_COMPLETION_ONLY,
		THREAD_TYPE_COMPUTE_ONLY,
		THREAD_TYPE_CALL_AND_COMPLETION,
		THREAD_TYPE_COMPLETION_AND_COMPUTE
	};

	enum ThreadTaskType
	{
		THREAD_TASK_READ_CALL,
		THREAD_TASK_COMPLETION,
		THREAD_TASK_COMPUTE
	};

	enum FileStatusType
	{
		FILE_STATUS_READ_CALL_TASK_WAITING,
		FILE_STATUS_READ_CALL_TASK_STARTED,
		FILE_STATUS_READ_CALL_TASK_COMPLETED,
		FILE_STATUS_COMPLETION_TASK_WAITING,
		FILE_STATUS_COMPLETION_TASK_STARTED,
		FILE_STATUS_COMPLETION_TASK_COMPLETED,
		FILE_STATUS_COMPUTE_TASK_WAITING,
		FILE_STATUS_COMPUTE_TASK_STARTED,
		FILE_STATUS_COMPUTE_TASK_COMPLTED
	};

	struct ThreadTaskArgs
	{
		UINT FID;
	};

	class FileLock
	{
	public:
		UINT status;
		HANDLE sem[3];

		explicit FileLock(UINT fid)
		{
			status = FILE_STATUS_READ_CALL_TASK_WAITING;
			for (int i = 0; i < 3; i++)
				sem[i] = CreateSemaphoreW(NULL, (UINT)(i == 0), 1, (std::to_wstring(fid) + L"-sem" + std::to_wstring(i)).c_str());
		}

		~FileLock()
		{
			for (int i = 0; i < 3; i++)
				CloseHandle(sem[i]);
		}
	};

	constexpr UINT g_threadCount = 8;
	constexpr UINT g_taskRemoveCount = 10;
	constexpr UINT g_exitCode = 99;
	constexpr BOOL g_fileFlag = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED;

	DWORD GetAlignedByteSize(PLARGE_INTEGER fileByteSize, DWORD sectorSize);

	void ReadCallTaskWork(UINT fid);
	void CompletionTaskWork(UINT fid);
	void ComputeTaskWork(UINT fid);

	DWORD HandleLockAcquireFailure(UINT fid, UINT threadTaskType);
	
	void DoThreadTask(ThreadTaskArgs* args, UINT threadTaskType, Concurrency::diagnostic::marker_series* workerSeries);
	DWORD WINAPI ThreadFunc(LPVOID param);
	
	void PostThreadTask(UINT t, UINT fid, UINT threadTaskType);
	void PostThreadExit(UINT t);

	void StartThreadTasks(UINT totalFileCount);
}
