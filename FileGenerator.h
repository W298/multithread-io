#pragma once

namespace FileGenerator
{
	enum FileDependencyType
	{
		FILE_DEPENDENCY_NONE,
		FILE_DEPENDENCY_ONLY_READ,
		FILE_DEPENDENCY_NEED_COMPUTE
	};

	UINT GenerateDummyFiles(const UINT depth, const UINT* fileCountAry, const UINT minByte, const UINT maxByte);
	void RemoveDummyFiles();
}
