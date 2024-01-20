#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

// #define FILE_GEN
// #define MMAP

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

	constexpr UINT testFileCount = 10000;

#ifdef MMAP

	UINT threadRole[4] = { 0, 0, 0, 0 };
	TestArgument args = { testFileCount, 24, threadRole, testFileCount, testFileCount, TRUE };
	TestResult res = StartThreadTasksFileMap(args);

#else

	UINT threadRole[4] = { 0, 12, 0, 12 };
	UINT totalThreadCount = 0;
	for (UINT i = 0; i < 4; i++)
		totalThreadCount += threadRole[i];

	TestArgument args = { testFileCount, totalThreadCount, threadRole, testFileCount, testFileCount, FALSE };
	TestResult res = StartThreadTasks(args);

#endif

	printf("\
File distribution: %s\n\
File size: %d ~ %d, Mean(%d), Variance(%d)\n\
File compute time: %f ms ~ %f ms, Mean(%f ms), Variance(%f ms)\n\
Total file size: %f MiB\n\n\
\
Thread count: %d\n\
Read Thread(%d) / Compute Thread(%d) / Compute-Read Thread(%d) / Both Thread(%d)\n\n\
Read Call Task Split(%d) / Compute Task Split(%d)\n\
\
Test file count: %d\n\
Peak commited Memory: %f MiB\n\
Elapsed time: %f ms\n\n\n\n\n\n",
"EXP",
fileSize.MinByte, fileSize.MaxByte, fileSize.Mean, fileSize.Variance,
fileCompute.MinMicroSeconds / 1024.0, fileCompute.MaxMicroSeconds / 1024.0, fileCompute.Mean / 1024.0, fileCompute.Variance / 1024.0,
res.totalFileSize / (1024.0 * 1024.0),
args.threadCount,
args.threadRole[0], args.threadRole[1], args.threadRole[2], args.threadRole[3],
args.readCallLimit, args.computeLimit,
testFileCount,
res.peakMem / (1024.0 * 1024.0),
res.elapsedMS);

	FILE* log;
	fopen_s(&log, "log.txt", "w");

	fprintf(log, "\
File distribution: %s\n\
File size: %d ~ %d, Mean(%d), Variance(%d)\n\
File compute time: %f ms ~ %f ms, Mean(%f ms), Variance(%f ms)\n\
Total file size: %f MiB\n\n\
\
Thread count: %d\n\
Read Thread(%d) / Compute Thread(%d) / Compute-Read Thread(%d) / Both Thread(%d)\n\n\
Read Call Task Split(%d) / Compute Task Split(%d)\n\
\
Test file count: %d\n\
Peak commited Memory: %f MiB\n\
Elapsed time: %f ms\n\n\n\n\n\n",
"EXP",
fileSize.MinByte, fileSize.MaxByte, fileSize.Mean, fileSize.Variance,
fileCompute.MinMicroSeconds / 1024.0, fileCompute.MaxMicroSeconds / 1024.0, fileCompute.Mean / 1024.0, fileCompute.Variance / 1024.0,
res.totalFileSize / (1024.0 * 1024.0),
args.threadCount,
args.threadRole[0], args.threadRole[1], args.threadRole[2], args.threadRole[3],
args.readCallLimit, args.computeLimit,
testFileCount,
res.peakMem / (1024.0 * 1024.0),
res.elapsedMS);

	fclose(log);

#endif

	return 0;
}