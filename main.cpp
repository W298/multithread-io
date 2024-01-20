#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

// #define FILE_GEN
// #define MMAP
// #define SYNC

using namespace FileGenerator;
using namespace ThreadSchedule;

int main()
{
	FileSizeArgs fileSize =
	{
		(UINT64)1 * 1024,								// MinByte
		(UINT64)2 * 1024 * 1024,						// MaxByte
		(UINT64)1 * 1024 * 1024,						// Mean
		(UINT64)512 * 1024								// Variance
	};
	FileComputeArgs fileCompute =
	{
		50u,								// MinMicroSeconds
		5000u,								// MaxMicroSeconds
		800u,								// Mean
		800u								// Variance
	};
	FileGenerationArgs fArgs =
	{
		10000,								// TotalFileCount
		fileSize,							// FileSize
		NORMAL_DIST,
		fileCompute,						// FileCompute
	};

#ifdef FILE_GEN
	GenerateDummyFiles(fArgs);
#else

	constexpr UINT testCount = 50;
	constexpr UINT testFileCount = 100;

	UINT64 totalFileSize = 0;
	double mean = 0;

	for (UINT t = 0; t < testCount; t++)
	{
#ifdef MMAP
		TestArgument args = { testFileCount, 24, NULL, testFileCount, testFileCount, TRUE };
		TestResult res = StartThreadTasksFileMap(args);
#endif

#ifdef SYNC
		TestArgument args = { testFileCount, 5, NULL, testFileCount, testFileCount, TRUE };
		TestResult res = StartSyncThreadTasks(args);
#endif

#ifndef MMAP
#ifndef SYNC
		UINT threadRole[4] = { 1, 4, 0, 0 };
		UINT totalThreadCount = 0;
		for (UINT i = 0; i < 4; i++)
			totalThreadCount += threadRole[i];

		TestArgument args = { testFileCount, totalThreadCount, threadRole, testFileCount, testFileCount, TRUE };
		TestResult res = StartThreadTasks(args);
#endif
#endif

		totalFileSize = res.totalFileSize;
		mean += res.elapsedMS;

		printf("\
Thread count: %d\n\
Read Thread(%d) / Compute Thread(%d) / Compute-Read Thread(%d) / Both Thread(%d)\n\
Peak commited Memory: %f MiB\n\
Elapsed time: %f ms\n\n",
args.threadCount,
args.threadRole == NULL ? 0 : args.threadRole[0],
args.threadRole == NULL ? 0 : args.threadRole[1],
args.threadRole == NULL ? 0 : args.threadRole[2],
args.threadRole == NULL ? 0 : args.threadRole[3],
res.peakMem / (1024.0 * 1024.0),
res.elapsedMS);

#endif
	}

	printf("\n\n\n\n\
File distribution: %s\n\
File size: %f KiB ~ %f KiB, Mean(%f KiB), Variance(%f KiB)\n\
File compute time: %f ms ~ %f ms, Mean(%f ms), Variance(%f ms)\n\
Test file count: %d\n\
Total file size: %f MiB\n\n",
fArgs.FileSizeModel == 0 ? "IDENTICAL" : fArgs.FileSizeModel == 1 ? "NORMAL_DIST" : "EXP",
fileSize.MinByte / 1024.0,
fileSize.MaxByte / 1024.0,
fileSize.Mean / 1024.0,
fileSize.Variance / 1024.0,
fileCompute.MinMicroSeconds / 1024.0,
fileCompute.MaxMicroSeconds / 1024.0,
fileCompute.Mean / 1024.0,
fileCompute.Variance / 1024.0,
testFileCount,
totalFileSize / (1024.0 * 1024.0)
);

	mean /= testCount;
	printf("%d Tested. Mean Elapsed time: %f ms\n\n\n\n", testCount, mean);

	return 0;
}