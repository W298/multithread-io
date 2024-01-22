#include "pch.h"
#include "FileGenerator.h"

void FileGenerator::GenerateDummyFiles(const FileGenerationArgs args)
{
	CreateDirectoryW(L"dummy", NULL);

	// Write distribution info file.
	{
		const HANDLE distFileHandle = 
			CreateFileW(
				L"dummy\\distribution",
				GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				FILE_FLAG_WRITE_THROUGH,
				NULL);

		BYTE* buffer = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(FileGenerationArgs)));
		memcpy(buffer, &args, sizeof(FileGenerationArgs));

		if (FALSE == WriteFile(distFileHandle, buffer, sizeof(FileGenerationArgs), NULL, NULL))
			THROW_ERROR(L"Failed to call WriteFile.");

		CloseHandle(distFileHandle);
		HeapFree(GetProcessHeap(), MEM_RELEASE, buffer);
	}

	std::random_device rd;
	std::mt19937 generator(rd());
	std::normal_distribution<double> sizeNormalDist(args.FileSize.Mean, args.FileSize.Variance);
	std::exponential_distribution<double> sizeExpDist(1.0 / args.FileSize.Mean);
	std::normal_distribution<double> computeNormalDist(args.FileCompute.Mean, args.FileCompute.Variance);

	// Generate dummy files.
	for (UINT fid = 0; fid < args.TotalFileCount; fid++)
	{
		if (fid % 1000 == 0)
			printf("%d th file creation completed.\n", fid);

		UINT64 fileByteSize = args.FileSize.MinByte;
		switch (args.FileSizeModel)
		{
		case IDENTICAL:
			fileByteSize = args.FileSize.Mean;
			break;
		case NORMAL_DIST:
			fileByteSize = sizeNormalDist(generator);
			break;
		case EXP:
			fileByteSize = sizeExpDist(generator);
			break;
		case SIZE_EVAL:
			fileByteSize = args.FileSize.MaxByte / pow(4, args.TotalFileCount - 1 - fid);
			break;
		}

		if (args.FileSizeModel != SIZE_EVAL)
			fileByteSize = max(args.FileSize.MinByte, min(args.FileSize.MaxByte, fileByteSize));

		UINT fileComputeTime = computeNormalDist(generator);
		fileComputeTime = max(args.FileCompute.MinMicroSeconds, min(args.FileCompute.MaxMicroSeconds, fileComputeTime));

		const HANDLE fileHandle = 
			CreateFileW(
				(L"dummy\\" + std::to_wstring(fid)).c_str(),
				GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				FILE_FLAG_WRITE_THROUGH,
				NULL);

		BYTE* buffer = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileByteSize));
		memcpy(buffer, &fileComputeTime, sizeof(UINT));

		if (FALSE == WriteFile(fileHandle, buffer, fileByteSize, NULL, NULL))
			THROW_ERROR(L"Failed to call WriteFile.");

		CloseHandle(fileHandle);
		HeapFree(GetProcessHeap(), MEM_RELEASE, buffer);
	}
}