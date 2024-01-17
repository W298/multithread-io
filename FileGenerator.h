#pragma once

namespace FileGenerator
{
	enum FileSizeModelType
	{
		IDENTICAL,
		NORMAL_DIST,
		EXP
	};

	struct FileSizeArgs
	{
		UINT64 MinByte;
		UINT64 MaxByte;
		UINT64 Mean;
		UINT64 Variance;
	};

	struct FileComputeArgs
	{
		UINT MinMicroSeconds;
		UINT MaxMicroSeconds;
		UINT Mean;
		UINT Variance;
	};
	
	struct FileGenerationArgs
	{
		UINT64 TotalFileCount;
		FileSizeArgs FileSize;
		FileSizeModelType FileSizeModel;
		FileComputeArgs FileCompute;
	};

	void GenerateDummyFiles(FileGenerationArgs fileGenerationArgs);
	void RemoveDummyFiles(UINT64 totalFileCount);
}
