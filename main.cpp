#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

using namespace FileGenerator;
using namespace ThreadSchedule;

int main()
{
	FileSizeArgs fileSize =
	{
		512u,						// MinByte
		1048576u,					// MaxByte
		4096u,						// Mean
		51200u						// Variance
	};
	FileComputeArgs fileCompute = 
	{
		50u,						// MinMicroSeconds
		5000u,						// MaxMicroSeconds
		400u,						// Mean
		400u						// Variance
	};
	FileDependencyArgs fileDep = 
	{
		FILE_DEPENDENCY_PYRAMID,	// Model
		5u,							// TreeDepth
		FALSE						// ForceAllDep
	};
	FileGenerationArgs fArgs = 
	{
		10000u,						// TotalFileCount
		fileSize,					// FileSize
		fileCompute,				// FileCompute
		fileDep						// FileDep
	};

	// GenerateDummyFiles(fArgs);

	constexpr UINT testFileCount = 1000;
	constexpr UINT testCount = 10;

	UINT rootFIDAry[testFileCount];
	for (int i = 0; i < testFileCount; i++)
		rootFIDAry[i] = i;
	
	double resultAry[testCount];
	for (int i = 0; i < testCount; i++)
		resultAry[i] = StartThreadTasks(rootFIDAry, testFileCount);

	// RemoveDummyFiles(248);

	return 0;
}