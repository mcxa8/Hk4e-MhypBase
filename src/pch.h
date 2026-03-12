#ifndef PCH_H
#define PCH_H

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "detours.h"
#pragma comment(lib, "vendor/detours/detours.lib")

#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>

#endif
