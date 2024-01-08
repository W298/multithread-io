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
		FILE_STATUS_HANDLE_CREATED,
		FILE_STATUS_IOCP_CREATED,
		FILE_STATUS_BUFFER_CREATED,
		FILE_STATUS_READ_FILE_CALLED,
		FILE_STATUS_COMPLETION_WAITING,
		FILE_STATUS_READ_COMPLETED,
		FILE_STATUS_COMPUTING,
		FILE_STATUS_COMPUTE_COMPLETED
	};

	struct ReadCallTaskArgs
	{
		UINT FID;
		size_t ByteSizeToRead;
	};

	struct CompletionTaskArgs
	{
		UINT FID;
	};

	struct ComputeTaskArgs
	{
		UINT FID;
	};

	constexpr UINT g_threadCount = 8;
	constexpr UINT g_exitCode = 99;

	DWORD WINAPI ThreadFunc(LPVOID param);
	void StartThreadTasks();
}