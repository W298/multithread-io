#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

using namespace FileGenerator;
using namespace ThreadSchedule;

void RunTest(const UINT testCount, const UINT testFileCount, const TestArgument args)
{
	// Open distribution info file.
	const HANDLE distFileHandle =
		CreateFileW(
			L"dummy\\distribution",
			GENERIC_READ,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

	BYTE* buffer = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(FileGenerationArgs)));

	if (FALSE == ReadFile(distFileHandle, buffer, sizeof(FileGenerationArgs), NULL, NULL) && GetLastError() != ERROR_IO_PENDING)
		THROW_ERROR(L"Failed to open distribution file.");

	FileGenerationArgs* fileGenArgs = reinterpret_cast<FileGenerationArgs*>(buffer);
	CloseHandle(distFileHandle);

	// Start test.
	UINT64 totalFileSize = 0;
	double elapsedTimeMean = 0;
	double peakMemoryMean = 0;

	for (UINT t = 0; t < testCount; t++)
	{
		TestResult res = StartTest(args);

		totalFileSize = res.TotalFileSize;
		elapsedTimeMean += res.ElapsedTime;
		peakMemoryMean += res.PeakMemory;

		printf("\
Peak memory: %.2f MiB\n\
Elapsed time: %.2f ms\n\n",
			res.PeakMemory / (1024.0 * 1024.0),
			res.ElapsedTime);
	}

	// Summarize test results.
	printf("\n\n\n\n\n\
[Test Result]\n\
File distribution: %s\n\
File size: %.2f KiB ~ %.2f KiB, Mean(%.2f KiB), Variance(%.2f KiB)\n\
File compute time: %.2f ms ~ %.2f ms, Mean(%.2f ms), Variance(%.2f ms)\n\
Test file count: %d\n\
Total file size: %.2f MiB\n\n\
Thread count: %d\n\
-- READCALL_ONLY(%d) / COMPUTE_ONLY(%d) / COMPUTE_AND_READCALL(%d) / READCALL_AND_COMPUTE(%d)\n\n\
Simulation type: %s\n\n",
		fileGenArgs->FileSizeModel == 0 ? "IDENTICAL" : fileGenArgs->FileSizeModel == 1 ? "NORMAL_DIST" : "EXP",
		fileGenArgs->FileSize.MinByte / 1024.0,
		fileGenArgs->FileSize.MaxByte / 1024.0,
		fileGenArgs->FileSize.Mean / 1024.0,
		fileGenArgs->FileSize.Variance / 1024.0,
		fileGenArgs->FileCompute.MinMicroSeconds / 1024.0,
		fileGenArgs->FileCompute.MaxMicroSeconds / 1024.0,
		fileGenArgs->FileCompute.Mean / 1024.0,
		fileGenArgs->FileCompute.Variance / 1024.0,
		testFileCount,
		totalFileSize / (1024.0 * 1024.0),
		args.ThreadCount,
		args.ThreadRoleAry == NULL ? 0 : args.ThreadRoleAry[0],
		args.ThreadRoleAry == NULL ? 0 : args.ThreadRoleAry[1],
		args.ThreadRoleAry == NULL ? 0 : args.ThreadRoleAry[2],
		args.ThreadRoleAry == NULL ? 0 : args.ThreadRoleAry[3],
		args.SimType == SIM_MANUAL_TASK_THREAD ? "SIM_MANUAL_TASK_THREAD" :
		args.SimType == SIM_ROLE_SPECIFIED_THREAD ? "SIM_ROLE_SPECIFIED_THREAD" :
		args.SimType == SIM_SYNC_THREAD ? "SIM_SYNC_THREAD" : "SIM_MMAP_THREAD");

	elapsedTimeMean /= testCount;
	peakMemoryMean /= testCount;

	printf("%d Tested.\nMean Elapsed time: %.2f ms\nMean Peak memory: %.2f MiB\n\n\n",
		testCount,
		elapsedTimeMean,
		peakMemoryMean / (1024.0 * 1024.0));

	HeapFree(GetProcessHeap(), MEM_RELEASE, buffer);
}

int main()
{
	/* --------------------------------------------------------------------- File Generation */
	
	FileSizeArgs fileSizeArgs =
	{
		(UINT64)1 * 1024,									// MinByte
		(UINT64)2 * 1024 * 1024,							// MaxByte
		(UINT64)1 * 1024 * 1024,							// Mean
		(UINT64)512 * 1024									// Variance
	};

	FileComputeArgs fileComputeArgs =
	{
		100u,												// MinMicroSeconds
		5000u,												// MaxMicroSeconds
		2000u,												// Mean
		2000u												// Variance
	};

	FileGenerationArgs fileGenArgs =
	{
		100,												// TotalFileCount
		fileSizeArgs,										// FileSize
		NORMAL_DIST,										// FileSizeModelType
		fileComputeArgs,									// FileCompute
	};

	// GenerateDummyFiles(fileGenArgs);
	
	/* -------------------------------------------------------------------------------------- */

	/* --------------------------------------------------------------------------------- Test */
	
	constexpr UINT testCount = 10;
	constexpr UINT testFileCount = 50;

	UINT threadRoleAry[4] = { 1, 2, 0, 0 };

	UINT threadCount = 0;
	for (UINT i = 0; i < 4; i++)
		threadCount += threadRoleAry[i];

	TestArgument args =
	{
		SIM_SYNC_THREAD,							// Simulation type
		testFileCount,							// Number of files to test
		threadCount,							// Number of threads
		threadRoleAry,							// Role of thread
		testFileCount,							// ReadCall task limit
		testFileCount,							// Compute task limit
		TRUE									// Use defined compute time?
	};
	
	RunTest(testCount, testFileCount, args);

	/* -------------------------------------------------------------------------------------- */

	return 0;
}