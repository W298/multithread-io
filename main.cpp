#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

using namespace FileGenerator;
using namespace ThreadSchedule;

int main()
{
	const UINT fileCountAry[5] = { 3u, 3u, 3u, 4u, 5u };
	GenerateDummyFiles(5u, fileCountAry, 512u, 1048576u);

	StartThreadTasks();

	RemoveDummyFiles();

	return 0;
}