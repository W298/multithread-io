#pragma once

namespace FileGenerator
{
	enum FileDependencyType
	{
		FILE_DEPENDENCY_NONE,
		FILE_DEPENDENCY_ONLY_READ,
		FILE_DEPENDENCY_NEED_COMPUTE
	};

	enum FileDependencyModel
	{
		FILE_DEPENDENCY_PYRAMID,
		FILE_DEPENDENCY_REVERSE_PYRAMID,
		FILE_DEPENDENCY_UNIFORM
	};

	struct FileSizeArgs
	{
		UINT64 minByte;
		UINT64 maxByte;
		UINT mean;
		UINT variance;
	};

	struct FileComputeArgs
	{
		UINT minMicroSeconds;
		UINT maxMicroSeconds;
		UINT mean;
		UINT variance;
	};

	struct FileDependencyArgs
	{
		FileDependencyModel model;
		UINT treeDepth;
		UINT treeMultiply;
	};
	
	struct FileGenerationArgs
	{
		FileSizeArgs fileSize;
		FileComputeArgs fileCompute;
		FileDependencyArgs fileDep;
	};

	UINT64 GenerateDummyFiles(const FileGenerationArgs fileGenerationArgs);
	void RemoveDummyFiles(UINT64 totalFileCount);
}
