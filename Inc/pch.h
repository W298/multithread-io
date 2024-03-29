#ifndef PCH_H
#define PCH_H

#include <iostream>
#include <windows.h>
#include <random>
#include <string>
#include <unordered_map>
#include <set>
#include <Psapi.h>

#ifdef _DEBUG
#include "cvmarkersobj.h"
#define SERIES_INIT(name) \
	marker_series series(name)
#define SPAN_INIT \
	span* s = nullptr
#define SPAN_START(cat, name, ...) \
	s = new span(series, cat, name, __VA_ARGS__)
#define SPAN_END \
    delete s
#endif

#define TIMER_INIT \
    LARGE_INTEGER freq; \
    LARGE_INTEGER st,en; \
    double el; \
    QueryPerformanceFrequency(&freq)

#define TIMER_START QueryPerformanceCounter(&st)

#define TIMER_STOP \
    QueryPerformanceCounter(&en); \
    el=(float)(en.QuadPart-st.QuadPart)/freq.QuadPart

#define TIMER_STOP_PRINT \
    QueryPerformanceCounter(&en); \
    el=(float)(en.QuadPart-st.QuadPart)/freq.QuadPart; \
    std::wcout<<el*1000<<L" ms\n"

#define SAFE_CLOSE_HANDLE(h) if (h != INVALID_HANDLE_VALUE) CloseHandle(h)

#define THROW_ERROR(msg) { MessageBox(NULL, msg, L"Critical Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL); ExitProcess(-1); }

#endif // PCH_H