#pragma once

namespace FileGenerator
{
	enum FileDependencyType
	{
		FILE_DEPENDENCY_NONE,
		FILE_DEPENDENCY_ONLY_READ,
		FILE_DEPENDENCY_NEED_COMPUTE
	};

	UINT64 GenerateDummyFiles(const UINT depth, const UINT* fileCountAry, const UINT64 minByte, const UINT64 maxByte);
	void RemoveDummyFiles();
}
