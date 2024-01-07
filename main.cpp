#include "pch.h"
#include "FileGenerator.h"
#include "Thread.h"

using namespace FileGenerator;
using namespace Thread;

int main()
{
	const UINT fileCountAry[5] = { 3, 3, 3, 4, 5 };
	GenerateDummyFiles(5, fileCountAry, 512u, 1048576u);

	system("pause");

	RemoveDummyFiles();

	// StartThreadTasks();

	return 0;
}