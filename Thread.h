#pragma once

namespace Thread
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

	struct ReadCallTaskArgs
	{
		HANDLE FileHandle;
		LPCWSTR FileName;
		size_t ByteSizeToRead;
	};

	constexpr UINT g_threadCount = 8;

	DWORD WINAPI ThreadFunc(LPVOID param);
	void StartThreadTasks();
}
