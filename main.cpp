#include "pch.h"
#include "FileGenerator.h"
#include "ThreadSchedule.h"

using namespace FileGenerator;
using namespace ThreadSchedule;

int main()
{
	const UINT fileCountAry[5] = { 50u, 50u, 50u, 50u, 50u };
	GenerateDummyFiles(5u, fileCountAry, 512u, 1048576u, 4096u, 51200u);

	StartThreadTasks();

	RemoveDummyFiles();

	return 0;
}