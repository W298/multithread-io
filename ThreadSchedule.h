#pragma once

namespace ThreadSchedule
{
	enum ThreadRoleType
	{
		THREAD_ROLE_READFILE_CALL_ONLY,
		THREAD_ROLE_COMPUTE_ONLY,
		THREAD_ROLE_COMPUTE_AND_READFILE,
		THREAD_ROLE_BOTH
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
		FILE_STATUS_COMPUTE_TASK_COMPLETED
	};

	enum SimulationType
	{
		SIM_UNIVERSAL_THREAD,
		SIM_ROLE_SPECIFIED_THREAD
	};

	struct ThreadTaskArgs
	{
		UINT FID;
	};

	struct TestArgument
	{
		UINT fileCount;
		UINT threadCount;
		UINT* threadRole;
		UINT readCallLimit;
		UINT computeLimit;
		BOOL enableComputeTime;
	};

	struct TestResult
	{
		double elapsedMiliseconds;
		UINT64 totalFileSize;
		SIZE_T peakMem;
	};

	class FileLock
	{
	public:
		HANDLE sem[3];
		HANDLE taskEndEv[3];

		explicit FileLock(UINT fid)
		{
			for (int i = 0; i < 3; i++)
			{
				sem[i] = CreateSemaphoreW(NULL, i == 0, 1, (std::to_wstring(fid) + L"-sem" + std::to_wstring(i)).c_str());
				taskEndEv[i] = CreateEvent(NULL, TRUE, FALSE, (std::to_wstring(fid) + L"-taskEndEv" + std::to_wstring(i)).c_str());
			}
		}

		~FileLock()
		{
			for (int i = 0; i < 3; i++)
			{
				CloseHandle(sem[i]);
				CloseHandle(taskEndEv[i]);
			}
		}
	};

	
	constexpr UINT g_taskRemoveCount = 10;
	constexpr UINT g_exitCode = 4294967295;
	constexpr BOOL g_fileFlag = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED;

	constexpr BOOL g_waitDependencyFront = TRUE;
	constexpr SimulationType g_simType = SIM_ROLE_SPECIFIED_THREAD;
	constexpr UINT g_computeLoopCount = 1;

	void FileMapTaskWork(UINT fid);
	void ReadCallTaskWork(UINT fid);
	void CompletionTaskWork(UINT fid);
	void ComputeTaskWork(UINT fid);

	DWORD HandleLockAcquireFailure(UINT fid, UINT threadTaskType, const Concurrency::diagnostic::marker_series& series);
	
	void DoThreadTask(ThreadTaskArgs* args, UINT threadTaskType, const Concurrency::diagnostic::marker_series& series);

	DWORD WINAPI UniversalThreadFunc(LPVOID param);
	DWORD WINAPI RoleSpecifiedThreadFunc(LPVOID param);
	DWORD WINAPI FileMapThreadFunc(LPVOID param);

	TestResult StartThreadTasks(TestArgument args);
	TestResult StartThreadTasksFileMap(TestArgument args);

	DWORD GetAlignedByteSize(PLARGE_INTEGER fileByteSize, DWORD sectorSize);
	void PostThreadTask(UINT t, UINT fid, UINT threadTaskType);
	void PostThreadExit(UINT t);
	void InsertThreadTaskFront(UINT t, const UINT* fidAry, const UINT* threadTaskTypeAry, UINT count);
}
