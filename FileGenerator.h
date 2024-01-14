#pragma once

namespace FileGenerator
{
	enum FileDependencyModel
	{
		FILE_DEPENDENCY_PYRAMID,
		FILE_DEPENDENCY_REVERSE_PYRAMID,
		FILE_DEPENDENCY_UNIFORM
	};

	struct FileSizeArgs
	{
		UINT64 MinByte;
		UINT64 MaxByte;
		UINT Mean;
		UINT Variance;
	};

	struct FileComputeArgs
	{
		UINT MinMicroSeconds;
		UINT MaxMicroSeconds;
		UINT Mean;
		UINT Variance;
	};

	struct FileDependencyArgs
	{
		FileDependencyModel Model;
		UINT TreeDepth;
		BOOL ForceAllDep;
	};
	
	struct FileGenerationArgs
	{
		UINT64 TotalFileCount;
		FileSizeArgs FileSize;
		FileComputeArgs FileCompute;
		FileDependencyArgs FileDep;
	};

	void GenerateDummyFiles(FileGenerationArgs fileGenerationArgs);
	void RemoveDummyFiles(UINT64 totalFileCount);
}
