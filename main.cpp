#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

using namespace FileGenerator;
using namespace ThreadSchedule;

int main()
{
	FileSizeArgs fileSize =
	{
		512u,								// MinByte
		10485760u,							// MaxByte
		1048576u,							// Mean
		1048576u							// Variance
	};
	FileComputeArgs fileCompute = 
	{
		50u,								// MinMicroSeconds
		5000u,								// MaxMicroSeconds
		400u,								// Mean
		400u								// Variance
	};
	FileDependencyArgs fileDep = 
	{
		FILE_DEPENDENCY_PYRAMID,			// Model
		5u,									// TreeDepth
		FALSE								// ForceAllDep
	};
	FileGenerationArgs fArgs = 
	{
		10000u,								// TotalFileCount
		fileSize,							// FileSize
		fileCompute,						// FileCompute
		fileDep								// FileDep
	};

	// GenerateDummyFiles(fArgs);

	constexpr UINT testFileCount = 10000;
	constexpr UINT testCount = 1;

	UINT rootFIDAry[testFileCount];
	for (int i = 0; i < testFileCount; i++)
		rootFIDAry[i] = i;
	
	UINT64 totalFileSize = 0;
	double resultAry[testCount];
	for (int i = 0; i < testCount; i++)
	{
		std::pair<double, UINT64> pair = StartThreadTasks(rootFIDAry, testFileCount);
		resultAry[i] = pair.first;
		totalFileSize = pair.second;
	}

	double mean = 0;
	for (int i = 0; i < testCount; i++)
		mean += resultAry[i];
	mean /= testCount;

	printf("\
File size: %d ~ %d, Mean(%d), Variance(%d)\n\
File compute time: %d ~ %d, Mean(%d), Variance(%d)\n\
Total file size: %f MiB\n\n\
\
Thread count: %d\n\
Read Thread(%d) / Compute Thread(%d) / Compute-Read Thread(%d) / Both Thread(%d)\n\n\
Read Call Task Split(%d) / Compute Task Split(%d)\n\
\
Test file count: %d\n\
Test count: %d\n\
Mean time: %f ms\n\n", 
		fileSize.MinByte, fileSize.MaxByte, fileSize.Mean, fileSize.Variance,
		fileCompute.MinMicroSeconds, fileCompute.MaxMicroSeconds, fileCompute.Mean, fileCompute.Variance,
		totalFileSize / (1024.0 * 1024.0),
		g_threadCount,
		g_threadRoleCount[0], g_threadRoleCount[1], g_threadRoleCount[2], g_threadRoleCount[3],
		g_readCallLimit, g_computeLimit,
		testFileCount,
		testCount,
		mean);

	for (int i = 0; i < testCount; i++)
		printf("%f ms\n", resultAry[i]);

	FILE* log;
	fopen_s(&log, "log.txt", "w");

	fprintf(log, "\
File size: %d ~ %d, Mean(%d), Variance(%d)\n\
File compute time: %d ~ %d, Mean(%d), Variance(%d)\n\
Total file size: %f MiB\n\n\
\
Thread count: %d\n\
Read Thread(%d) / Compute Thread(%d) / Compute-Read Thread(%d) / Both Thread(%d)\n\n\
Read Call Task Split(%d) / Compute Task Split(%d)\n\
\
Test file count: %d\n\
Test count: %d\n\
Mean time: %f ms\n\n",
		fileSize.MinByte, fileSize.MaxByte, fileSize.Mean, fileSize.Variance,
		fileCompute.MinMicroSeconds, fileCompute.MaxMicroSeconds, fileCompute.Mean, fileCompute.Variance,
		totalFileSize / (1024.0 * 1024.0),
		g_threadCount,
		g_threadRoleCount[0], g_threadRoleCount[1], g_threadRoleCount[2], g_threadRoleCount[3],
		g_readCallLimit, g_computeLimit,
		testFileCount,
		testCount,
		mean);

	for (int i = 0; i < testCount; i++)
		fprintf(log, "%f ms\n", resultAry[i]);

	fclose(log);

	// RemoveDummyFiles(248);

	return 0;
}