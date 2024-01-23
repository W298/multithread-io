# :toolbox: multithread-io

Multithread I/O simulation on Windows API.  
Mainly focus on testing multithread & overlapped I/O on various enviornment (number of files, file size, compute time...)

All test result and analyze is located at [wiki page](https://github.com/W298/multithread-io/wiki)

Profiling code is inserted with [Concurrency Visualizer](https://learn.microsoft.com/en-us/visualstudio/profiling/concurrency-visualizer?view=vs-2022) SDK.

*On Debug build, you must install Concurrency Visualizer Extension, and add SDK to project.*  
*On Release build, you don't have to install and include it.*

## Test Coverage

1. Performance Evaluation
    - [Overlapped IO Performance](https://github.com/W298/multithread-io/wiki/Overlapped-IO-Performance)
    - [Read Call Task Overhead](https://github.com/W298/multithread-io/wiki/Read-Call-Task-Overhead)

2. Compare Thread / Task Scheduling
    - [Multithread Sync vs Overlapped IO](https://github.com/W298/multithread-io/wiki/Multithread-Sync-vs-Overlapped-IO)
    - Overlapped IO
        - [Do Read Call Task as soon as possible](https://github.com/W298/multithread-io/wiki/Do-Read-Call-Task-as-soon-as-possible)
        - [Role Speficied Thread](https://github.com/W298/multithread-io/wiki/Role-Speficied-Thread)
    - [Memory Mapped File](https://github.com/W298/multithread-io/wiki/Memory-Mapped-File)
3. Stress test
    - [Massive File Count](https://github.com/W298/multithread-io/wiki/Massive-File-Count)
4. [Summary](https://github.com/W298/multithread-io/wiki/Summary)
