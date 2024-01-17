#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

using namespace FileGenerator;
using namespace ThreadSchedule;

int main()
{
	FileSizeArgs fileSize =
	{
		(UINT64)512,								// MinByte
		(UINT64)512 * 1024 * 1024,					// MaxByte
		(UINT64)512 * 1024 * 1024,					// Mean
		(UINT64)1 * 1024 * 1024						// Variance
	};
	FileComputeArgs fileCompute = 
	{
		50u,								// MinMicroSeconds
		5000u,								// MaxMicroSeconds
		400u,								// Mean
		400u								// Variance
	};
	FileGenerationArgs fArgs = 
	{
		40u,								// TotalFileCount
		fileSize,							// FileSize
		IDENTICAL,
		fileCompute,						// FileCompute
	};

	// GenerateDummyFiles(fArgs);

	constexpr UINT testFileCount = 40;

	UINT rootFIDAry[testFileCount];
	for (int i = 0; i < testFileCount; i++)
		rootFIDAry[i] = i;
	
	TestArgument args = { rootFIDAry, testFileCount, 4, 40, 40 };
	TestResult res = StartThreadTasks(args);

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
	0, 0, 0, args.threadCount,
	args.readCallLimit, args.computeLimit,
	testFileCount,
	res.peakMem / (1024.0 * 1024.0),
	res.elapsedMiliseconds);

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
	0, 0, 0, args.threadCount,
	args.readCallLimit, args.computeLimit,
	testFileCount,
	res.peakMem / (1024.0 * 1024.0),
	res.elapsedMiliseconds);

	fclose(log);

	// RemoveDummyFiles(248);

	return 0;
}