#include "pch.h"
#include "FileGenerator.h"

using namespace Concurrency::diagnostic;

UINT64 FileGenerator::GenerateDummyFiles(const FileGenerationArgs args)
{
	marker_series mainThreadSeries(_T("Main Thread - FileGenerator"));
	span* s = new span(mainThreadSeries, 0, _T("File Generation"));

	UINT* fileCountAry = new UINT[args.fileDep.treeDepth];
	UINT64 totalFileCount = 0;

	UINT beforeDepthCount = 1u;
	for (UINT i = 0; i < args.fileDep.treeDepth; i++)
	{
		fileCountAry[i] = beforeDepthCount * args.fileDep.treeMultiply;
		totalFileCount += fileCountAry[i];
		beforeDepthCount = fileCountAry[i];
	}

	CreateDirectoryW(L"dummy", NULL);
	const std::wstring path = L"dummy\\";

	HANDLE* handleAry = new HANDLE[totalFileCount];
	BYTE* buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, args.fileSize.maxByte);

	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution uniformIntDist(0, 2);
	std::normal_distribution<double> sizeNormalDist(args.fileSize.mean, args.fileSize.variance);
	std::normal_distribution<double> computeNormalDist(args.fileCompute.mean, args.fileCompute.variance);

	UINT fidOffset = 0;
	for (UINT curDepth = 0; curDepth < args.fileDep.treeDepth; curDepth++)
	{
		for (UINT fid = fidOffset; fid < fidOffset + fileCountAry[curDepth]; fid++)
		{
			// File creation.
			const UINT64 fileByteSize = max(args.fileSize.minByte, min(args.fileSize.maxByte, round(abs(sizeNormalDist(generator)))));
			const UINT fileComputeTime = max(args.fileCompute.minMicroSeconds, min(args.fileCompute.maxMicroSeconds, round(abs(computeNormalDist(generator)))));
			
			handleAry[fid] = CreateFileW((path + std::to_wstring(fid)).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);

			BYTE* writeAddress = buffer;

			// Write compute time to file.
			memcpy(writeAddress, &fileComputeTime, sizeof(UINT));
			writeAddress += sizeof(UINT);

			if (curDepth < args.fileDep.treeDepth - 1) // Exclude leaf file.
			{
				// File dependency define.
				std::vector<std::pair<UINT, BYTE>> dependencyVec;
				for (UINT off = 0; off < fileCountAry[curDepth + 1]; off++)
				{
					const UINT dependencyFID = fidOffset + fileCountAry[curDepth] + off;
					const BYTE fileDependency = uniformIntDist(generator);

					if (fileDependency != 0)
						dependencyVec.emplace_back(dependencyFID, fileDependency);
				}

				// Write dependency pair count to file.
				const UINT dependencyPairCount = dependencyVec.size();
				memcpy(writeAddress, &dependencyPairCount, sizeof(UINT));
				writeAddress += sizeof(UINT);

				// Write dependency to file.
				for (UINT i = 0; i < dependencyVec.size(); i++)
				{
					const UINT dependencyFID = dependencyVec[i].first;
					const BYTE fileDependency = dependencyVec[i].second;

					memcpy(writeAddress, &dependencyFID, sizeof(UINT));
					writeAddress += sizeof(UINT);

					memcpy(writeAddress, &fileDependency, sizeof(BYTE));
					writeAddress += sizeof(BYTE);
				}

				WriteFile(handleAry[fid], buffer, fileByteSize, NULL, NULL);
				ZeroMemory(buffer, writeAddress - buffer);
			}
			else
			{
				WriteFile(handleAry[fid], buffer, fileByteSize, NULL, NULL);
				ZeroMemory(buffer, writeAddress - buffer);
			}
		}

		fidOffset += fileCountAry[curDepth];
	}

	// Release memory.
	for (UINT64 fid = 0; fid < totalFileCount; fid++)
	{
		CloseHandle(handleAry[fid]);
	}
	HeapFree(GetProcessHeap(), MEM_RELEASE, buffer);
	delete[] handleAry;

	delete s;
	return totalFileCount;
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