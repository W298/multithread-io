#pragma once

namespace ThreadSchedule
{
	enum ThreadRoleType
	{
		THREAD_ROLE_READCALL_ONLY,
		THREAD_ROLE_COMPUTE_ONLY,
		THREAD_ROLE_COMPUTE_AND_READCALL,
		THREAD_ROLE_READCALL_AND_COMPUTE
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
		SIM_MANUAL_TASK_THREAD,
		SIM_ROLE_SPECIFIED_THREAD,
		SIM_SYNC_THREAD,
		SIM_MMAP_THREAD,
	};

	struct ThreadTaskArgs
	{
		UINT FID;
	};

	struct TestArgument
	{
		SimulationType SimType;
		UINT TestFileCount;
		UINT ThreadCount;
		UINT* ThreadRoleAry;
		UINT ReadCallTaskLimit;
		UINT ComputeTaskLimit;
		BOOL UseDefinedComputeTime;
	};

	struct TestResult
	{
		double ElapsedTime;
		UINT64 TotalFileSize;
		SIZE_T PeakMemory;
	};

	struct TaskMode
	{
		UINT ReadCallTaskCount;
		UINT ComputeTaskCount;
		BOOL IsComputeTaskTurn;
	};

	class FileLock
	{
	public:
		HANDLE semLock[3];
		HANDLE taskEndEvent[3];

		explicit FileLock(UINT fid)
		{
			for (UINT i = 0; i < 3; i++)
			{
				semLock[i] = 
					CreateSemaphoreW(
						NULL, 
						i == 0, 
						1, 
						(std::to_wstring(fid) + L"-sem" + std::to_wstring(i)).c_str());
				taskEndEvent[i] = 
					CreateEvent(
						NULL, 
						TRUE, 
						FALSE, 
						(std::to_wstring(fid) + L"-taskEndEv" + std::to_wstring(i)).c_str());
			}
		}

		~FileLock()
		{
			for (UINT i = 0; i < 3; i++)
			{
				CloseHandle(semLock[i]);
				CloseHandle(taskEndEvent[i]);
			}
		}
	};
	
	constexpr UINT g_exitCode = 4294967295;
	constexpr BOOL g_fileFlag = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED;
	
	// Define loop count of checksum (compute task)
	// Only affects when UseDefinedComputeTime is FALSE.
	constexpr UINT g_computeLoopCount = 1;

	// Define how much tasks will be dequeued at once.
	// Only affects when simulation type is MANUAL.
	constexpr UINT g_taskRemoveCount = 10;
	
	// Prepare file handle and do ReadFile Call.
	// Only used when simulation type is MANUAL or ROLE_SPECIFIED.
	void ReadCallTaskWork(UINT fid);
	
	// Wait until file read is done using IOCP.
	// Only used when simulation type is MANUAL only.
	void CompletionTaskWork(UINT fid);
	
	// Do compute task and release resources.
	// Only used when simulation type is MANUAL or ROLE_SPECIFIED.
	void ComputeTaskWork(UINT fid);
	
	// Do ReadCall task, Compute task once.
	// Only used when simulation type is MMAP.
	void MMAPTaskWork(UINT fid);

	// Do ReadCall task, Compute task once.
	// Only used when simulation type is SYNC.
	void SyncTaskWork(UINT fid);

	// Failed to get lock, which means another thread is processing task.
	// Only used when simulation type is MANUAL.
	DWORD HandleLockAcquireFailure(const UINT fid, const UINT threadTaskType);
	
	// Do tasks with task type.
	// Only used when simulation type is MANUAL.
	void DoThreadTask(ThreadTaskArgs* args, const UINT threadTaskType);

	DWORD WINAPI ManualThreadFunc(LPVOID param);
	DWORD WINAPI RoleSpecifiedThreadFunc(LPVOID param);
	DWORD WINAPI MMAPThreadFunc(LPVOID param);
	DWORD WINAPI SyncThreadFunc(LPVOID param);

	TestResult StartTest(TestArgument args);

	// Get aligned byte size using sector size.
	DWORD GetAlignedByteSize(PLARGE_INTEGER fileByteSize, DWORD sectorSize);
	
	// Post task to thread.
	void PostThreadTask(UINT t, UINT fid, UINT threadTaskType);
	
	// Post exit code to thread.
	void PostThreadExit(UINT t);
}
