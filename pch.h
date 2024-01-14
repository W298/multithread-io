#ifndef PCH_H
#define PCH_H

#include <iostream>
#include <windows.h>
#include <random>
#include <string>
#include <unordered_map>

#include "cvmarkersobj.h"

#define SERIES_INIT(name) \
	marker_series series(name)
#define SPAN_INIT \
	span* s = nullptr
#define SPAN_START(cat, name, ...) \
	s = new span(series, cat, name, __VA_ARGS__)
#define SPAN_END delete s

#endif // PCH_H