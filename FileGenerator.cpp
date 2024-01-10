#include "pch.h"
#include "FileGenerator.h"

using namespace Concurrency::diagnostic;

UINT64 g_totalFileCount = 0;

UINT64 FileGenerator::GenerateDummyFiles(
	const UINT depth, const UINT* fileCountAry, const UINT64 minByte, const UINT64 maxByte, 
	const UINT mean, const UINT variance)
{
	marker_series mainThreadSeries(_T("Main Thread - FileGenerator"));
	span* s = new span(mainThreadSeries, 0, _T("File Generation"));

	UINT64 totalFileCount = 0;
	for (UINT i = 0; i < depth; i++)
		totalFileCount += fileCountAry[i];

	g_totalFileCount = totalFileCount;

	CreateDirectoryW(L"dummy", NULL);
	const std::wstring path = L"dummy\\";

	HANDLE* handleAry = new HANDLE[totalFileCount];
	BYTE* buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, maxByte);

	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution uniformIntDist(0, 2);
	std::normal_distribution<double> normalDist(mean, variance);

	UINT fidOffset = 0;
	for (UINT curDepth = 0; curDepth < depth; curDepth++)
	{
		for (UINT fid = fidOffset; fid < fidOffset + fileCountAry[curDepth]; fid++)
		{
			// File creation.
			const UINT64 fileByteSize = max(minByte, min(maxByte, round(abs(normalDist(generator)))));
			handleAry[fid] = CreateFileW((path + std::to_wstring(fid)).c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL);

#ifdef PRINT_FILE_GEN
			std::cout << "FID:" << fid << "\t" << fileByteSize << "\n";
#endif

			if (curDepth < depth - 1) // Exclude leaf file.
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
				memcpy(buffer, &dependencyPairCount, sizeof(UINT));

				// Write dependency to file.
				for (UINT i = 0; i < dependencyVec.size(); i++)
				{
					const UINT dependencyFID = dependencyVec[i].first;
					const BYTE fileDependency = dependencyVec[i].second;

#ifdef PRINT_FILE_GEN
					std::cout << dependencyFID << "(" << (int)fileDependency << ")" << " ";
#endif

					BYTE* writeAddress = buffer + sizeof(UINT) + i * (sizeof(UINT) + sizeof(BYTE));
					memcpy(writeAddress, &dependencyFID, sizeof(UINT));
					memcpy(writeAddress + sizeof(UINT), &fileDependency, sizeof(BYTE));
				}
#ifdef PRINT_FILE_GEN
				std::cout << "\n";
#endif

				WriteFile(handleAry[fid], buffer, fileByteSize, NULL, NULL);
				ZeroMemory(buffer, sizeof(UINT) + dependencyVec.size() * (sizeof(UINT) + sizeof(BYTE)));
			}
			else
			{
				WriteFile(handleAry[fid], buffer, fileByteSize, NULL, NULL);
			}
		}

		fidOffset += fileCountAry[curDepth];
	}

	// Release memory.
	for (UINT64 fid = 0; fid < totalFileCount; fid++)
	{
		if (handleAry[fid] != INVALID_HANDLE_VALUE)
			CloseHandle(handleAry[fid]);
	}
	HeapFree(GetProcessHeap(), MEM_RELEASE, buffer);
	delete[] handleAry;

	delete s;
	return totalFileCount;
}

void FileGenerator::RemoveDummyFiles()
{
	for (UINT fid = 0; fid < g_totalFileCount; fid++)
	{
		std::wstring path = L"dummy\\";
		DeleteFileW((path + std::to_wstring(fid)).c_str());
	}
	RemoveDirectoryW(L"dummy");
}