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
		250u,						// TotalFileCount
		fileSize,					// FileSize
		fileCompute,				// FileCompute
		fileDep						// FileDep
	};

	// GenerateDummyFiles(fArgs);

	UINT* rootFIDAry = new UINT[248];
	for (int i = 0; i < 248; i++)
		rootFIDAry[i] = i;

	StartThreadTasks(rootFIDAry, 248);

	delete[] rootFIDAry;

	// RemoveDummyFiles(totalFileCount);

	return 0;
}