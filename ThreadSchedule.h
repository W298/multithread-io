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
		FILE_STATUS_NON_ACCESS,
		FILE_STATUS_READ_FILE_CALLED,
		FILE_STATUS_READ_COMPLETED,
		FILE_STATUS_COMPUTE_COMPLETED
	};

	struct ThreadTaskArgs
	{
		UINT FID;
	};

	class FileLock
	{
	public:
		HANDLE sem1;
		HANDLE sem2;
		HANDLE sem3;
		UINT status;

		FileLock() = default;
		FileLock(UINT fid)
		{
			status = FILE_STATUS_NON_ACCESS;
			sem1 = CreateSemaphoreW(NULL, 1, 1, (std::to_wstring(fid) + L"-sem1").c_str());
			sem2 = CreateSemaphoreW(NULL, 0, 1, (std::to_wstring(fid) + L"-sem2").c_str());
			sem3 = CreateSemaphoreW(NULL, 0, 1, (std::to_wstring(fid) + L"-sem3").c_str());
		}
	};

	constexpr UINT g_threadCount = 3;
	constexpr UINT g_exitCode = 99;

	DWORD WINAPI ThreadFunc(LPVOID param);
	void StartThreadTasks();
}
