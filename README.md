# multithread-io

Multithread I/O simulation on Windows API.  
Mainly focus on testing multithread & overlapped io on various enviornment (number of files, file size, computation time...)

All test result and analyze is located at [description page](https://github.com/W298/multithread-io/blob/main/docs/Description.md)

Profiling code is inserted with [Concurrency Visualizer](https://learn.microsoft.com/en-us/visualstudio/profiling/concurrency-visualizer?view=vs-2022) SDK.

## Test Coverage

1. Overlapped I/O vs Sync I/O Performance
2. `ReadFile` overhead evaluation
3. Task scheduling with multithread & overlapped I/O
    - Do all ReadFile tasks first, and then do all Compute tasks
    - Role-specified thread (ReadFile task only, Compute task only...)
    - Memory-Mapped File
