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
		HANDLE sem1;
		HANDLE sem2;
		HANDLE sem3;

		FileLock() : status(FILE_STATUS_READ_CALL_TASK_WAITING), sem1(NULL), sem2(NULL), sem3(NULL) {}
		explicit FileLock(UINT fid)
		{
			status = FILE_STATUS_READ_CALL_TASK_WAITING;
			sem1 = CreateSemaphoreW(NULL, 1, 1, (std::to_wstring(fid) + L"-sem1").c_str());
			sem2 = CreateSemaphoreW(NULL, 0, 1, (std::to_wstring(fid) + L"-sem2").c_str());
			sem3 = CreateSemaphoreW(NULL, 0, 1, (std::to_wstring(fid) + L"-sem3").c_str());
		}

		~FileLock()
		{
			CloseHandle(sem1);
			CloseHandle(sem2);
			CloseHandle(sem3);
		}
	};

	constexpr UINT testFileCount = 128;
	constexpr UINT g_threadCount = 8;
	constexpr UINT g_exitCode = 99;
	constexpr BOOL g_fileFlag = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED;

	DWORD GetAlignedByteSize(PLARGE_INTEGER fileByteSize, DWORD sectorSize);

	void ReadCallTaskWork(UINT fid);
	void CompletionTaskWork(UINT fid);
	void ComputeTaskWork(double timeOverMiliseconds);

	DWORD HandleLockAcquireFailure(UINT fid, UINT threadTaskType);
	
	void DoThreadTask(ThreadTaskArgs* args, UINT threadTaskType, 
		Concurrency::diagnostic::marker_series* workerSeries,
		Concurrency::diagnostic::marker_series* statusSeries
	);
	DWORD WINAPI ThreadFunc(LPVOID param);
	
	void PostThreadTask(UINT t, UINT fid, UINT threadTaskType);
	void PostThreadExit(UINT t);

	void StartThreadTasks();
}
