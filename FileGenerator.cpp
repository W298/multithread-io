#include "pch.h"
#include "FileGenerator.h"

using namespace Concurrency::diagnostic;

void FileGenerator::GenerateDummyFiles(const FileGenerationArgs args)
{
	SERIES_INIT(_T("Main Thread FileGenerator"));
	SPAN_INIT;

	SPAN_START(0, _T("File Generation"));

	CreateDirectoryW(L"dummy", NULL);
	const LPCTSTR path = L"dummy\\";

	BYTE* buffer = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, args.FileSize.MaxByte));

	std::random_device rd;
	std::mt19937 generator(rd());
	std::normal_distribution<double> sizeNormalDist(args.FileSize.Mean, args.FileSize.Variance);
	std::exponential_distribution<double> sizeExpDist(1.0 / args.FileSize.Mean);
	std::normal_distribution<double> computeNormalDist(args.FileCompute.Mean, args.FileCompute.Variance);

	for (UINT fid = 0; fid < args.TotalFileCount; fid++)
	{
		if (fid % 1000 == 0)
			std::cout << fid << "\n";

		// File creation.
		UINT64 fileByteSize = args.FileSize.MinByte;
		switch (args.FileSizeModel)
		{
		case IDENTICAL:
			fileByteSize = args.FileSize.Mean;
			break;
		case NORMAL_DIST:
			fileByteSize = max(args.FileSize.MinByte, min(args.FileSize.MaxByte, round(abs(sizeNormalDist(generator)))));
			break;
		case EXP:
			fileByteSize = max(args.FileSize.MinByte, min(args.FileSize.MaxByte, round(abs(sizeExpDist(generator)))));
			break;
		}

		const UINT fileComputeTime =
			max(args.FileCompute.MinMicroSeconds, min(args.FileCompute.MaxMicroSeconds, round(abs(computeNormalDist(generator)))));

		const HANDLE handle = CreateFileW(
			(path + std::to_wstring(fid)).c_str(),
			GENERIC_WRITE,
			0,
			NULL,
			CREATE_ALWAYS,
			FILE_FLAG_WRITE_THROUGH,
			NULL);

		// Write compute time to file.
		memcpy(buffer, &fileComputeTime, sizeof(UINT));
		WriteFile(handle, buffer, fileByteSize, NULL, NULL);
		
		ZeroMemory(buffer, sizeof(UINT));

		CloseHandle(handle);
	}	
	
	HeapFree(GetProcessHeap(), MEM_RELEASE, buffer);

	SPAN_END;
}

void FileGenerator::RemoveDummyFiles(UINT64 totalFileCount)
{
	for (UINT fid = 0; fid < totalFileCount; fid++)
	{
		std::wstring path = L"dummy\\";
		DeleteFileW((path + std::to_wstring(fid)).c_str());
	}
	RemoveDirectoryW(L"dummy");
}