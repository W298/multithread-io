#include "pch.h"
#include "FileGenerator.h"

UINT g_totalFileCount = 0;

UINT FileGenerator::GenerateDummyFiles(const UINT depth, const UINT* fileCountAry, const UINT minByte, const UINT maxByte)
{
	UINT totalFileCount = 0;
	for (int i = 0; i < depth; i++)
		totalFileCount += fileCountAry[i];

	g_totalFileCount = totalFileCount;

	CreateDirectoryW(L"dummy", NULL);
	const std::wstring path = L"dummy\\";

	HANDLE* handleAry = new HANDLE[totalFileCount];
	BYTE* buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, maxByte);

	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution uniformIntDist(0, 2);
	std::normal_distribution normalDist(4096.0, 51200.0);

	UINT fidOffset = 0;
	for (int d = 0; d < depth; d++)
	{
		for (int fid = fidOffset; fid < fidOffset + fileCountAry[d]; fid++)
		{
			// File creation.
			const UINT fileByteSize = max(minByte, min(maxByte, round(abs(normalDist(generator)))));
			handleAry[fid] = CreateFileW((path + std::to_wstring(fid)).c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL);

			std::cout << "FID:" << fid << "\t" << fileByteSize << "\n";

			if (d < depth - 1) // Exclude leaf file.
			{
				// File dependency define.
				std::vector<std::pair<UINT, BYTE>> dependencyVec;
				for (int off = 0; off < fileCountAry[d + 1]; off++)
				{
					const UINT dependencyFID = fidOffset + fileCountAry[d] + off;
					const BYTE fileDependency = uniformIntDist(generator);

					if (fileDependency != 0)
						dependencyVec.emplace_back(dependencyFID, fileDependency);
				}

				// Write dependency pair count to file.
				const UINT dependencyPairCount = dependencyVec.size();
				memcpy(buffer, &dependencyPairCount, sizeof(UINT));

				// Write dependency to file.
				for (int i = 0; i < dependencyVec.size(); i++)
				{
					const UINT dependencyFID = dependencyVec[i].first;
					const BYTE fileDependency = dependencyVec[i].second;

					std::cout << dependencyFID << "(" << (int)fileDependency << ")" << " ";

					BYTE* writeAddress = buffer + sizeof(UINT) + i * (sizeof(UINT) + sizeof(BYTE));
					memcpy(writeAddress, &dependencyFID, sizeof(UINT));
					memcpy(writeAddress + sizeof(UINT), &fileDependency, sizeof(BYTE));
				}
				std::cout << "\n";

				WriteFile(handleAry[fid], buffer, fileByteSize, NULL, NULL);
				ZeroMemory(buffer, sizeof(UINT) + dependencyVec.size() * (sizeof(UINT) + sizeof(BYTE)));
			}
			else
			{
				WriteFile(handleAry[fid], buffer, fileByteSize, NULL, NULL);
			}
		}

		fidOffset += fileCountAry[d];
	}

	// Release memory.
	for (int f = 0; f < totalFileCount; f++)
	{
		if (handleAry[f] != INVALID_HANDLE_VALUE)
			CloseHandle(handleAry[f]);
	}
	HeapFree(GetProcessHeap(), MEM_RELEASE, buffer);
	delete[] handleAry;

	return totalFileCount;
}

void FileGenerator::RemoveDummyFiles()
{
	for (int f = 0; f < g_totalFileCount; f++)
	{
		std::wstring path = L"dummy\\";
		DeleteFileW((path + std::to_wstring(f)).c_str());
	}
	RemoveDirectoryW(L"dummy");
}