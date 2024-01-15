#pragma once

namespace ThreadSchedule
{
	enum ThreadRoleType
	{
		NONE						= 0x00000000,
		THREAD_ROLE_READ_FILE_CALL	= 0x00000001,
		THREAD_ROLE_COMPUTE			= 0x00000010,
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

	constexpr UINT g_threadCount = 14;
	constexpr UINT g_taskRemoveCount = 10;
	constexpr UINT g_exitCode = 4294967295;
	constexpr BOOL g_fileFlag = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED;

	constexpr BOOL g_waitDependencyFront = TRUE;
	constexpr SimulationType g_simType = SIM_ROLE_SPECIFIED_THREAD;
	
	constexpr UINT g_readThreadCount = 2;				// Only avaliable on SIM_ROLE_SPECIFIED_THREAD.
	constexpr BOOL g_computeThreadDoReadTask = TRUE;	// Only avaliable on SIM_ROLE_SPECIFIED_THREAD.

	void ReadCallTaskWork(UINT fid);
	void CompletionTaskWork(UINT fid);
	void ComputeTaskWork(UINT fid);

	DWORD HandleLockAcquireFailure(UINT fid, UINT threadTaskType, const Concurrency::diagnostic::marker_series& series);
	
	void DoThreadTask(ThreadTaskArgs* args, UINT threadTaskType, const Concurrency::diagnostic::marker_series& series);

	DWORD WINAPI UniversalThreadFunc(LPVOID param);
	DWORD WINAPI RoleSpecifiedThreadFunc(LPVOID param);

	double StartThreadTasks(const UINT* rootFIDAry, UINT rootFIDAryCount);

	DWORD GetAlignedByteSize(PLARGE_INTEGER fileByteSize, DWORD sectorSize);
	void PostThreadTask(UINT t, UINT fid, UINT threadTaskType);
	void PostThreadExit(UINT t);
	void InsertThreadTaskFront(UINT t, const UINT* fidAry, const UINT* threadTaskTypeAry, UINT count);
}
