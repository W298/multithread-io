#ifndef PCH_H
#define PCH_H

#include <iostream>
#include <windows.h>
#include <random>
#include <string>
#include <unordered_map>
#include <set>
#include <Psapi.h>

#include "cvmarkersobj.h"

#define SERIES_INIT(name) \
	marker_series series(name)
#define SPAN_INIT \
	span* s = nullptr
#define SPAN_START(cat, name, ...) \
	s = new span(series, cat, name, __VA_ARGS__)
#define SPAN_END \
    delete s

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

#endif // PCH_H