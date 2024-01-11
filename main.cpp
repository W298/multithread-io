#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

using namespace FileGenerator;
using namespace ThreadSchedule;

int main()
{
	FileSizeArgs fileSize = FileSizeArgs(512u, 1048576u, 4096u, 51200u);
	FileComputeArgs fileCompute = FileComputeArgs(50u, 5000u, 400u, 400u);
	FileDependencyArgs fileDep = FileDependencyArgs(FILE_DEPENDENCY_PYRAMID, 4u, TRUE, FILE_DEPENDENCY_ONLY_READ);
	FileGenerationArgs fArgs = FileGenerationArgs(128u, fileSize, fileCompute, fileDep);
	GenerateDummyFiles(fArgs);

	// StartThreadTasks(1);

	// RemoveDummyFiles(totalFileCount);

	return 0;
}