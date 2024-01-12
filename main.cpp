#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

using namespace FileGenerator;
using namespace ThreadSchedule;

int main()
{
	FileSizeArgs fileSize = FileSizeArgs(512u, 1048576u, 4096u, 51200u);
	FileComputeArgs fileCompute = FileComputeArgs(50u, 5000u, 400u, 400u);
	FileDependencyArgs fileDep = FileDependencyArgs(FILE_DEPENDENCY_PYRAMID, 5u, FALSE);
	FileGenerationArgs fArgs = FileGenerationArgs(250u, fileSize, fileCompute, fileDep);
	// GenerateDummyFiles(fArgs);

	UINT rootFIDAry[9] = { 4, 5, 6, 7, 8, 9, 10, 11, 12 };
	StartThreadTasks(rootFIDAry, 9);

	// RemoveDummyFiles(totalFileCount);

	return 0;
}