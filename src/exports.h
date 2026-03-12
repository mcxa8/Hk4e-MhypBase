#pragma once

#include <Windows.h>

extern "C" FARPROC OriginalFuncs_version[17];

inline constexpr const char* ExportNames_version[] = {
	"GetFileVersionInfoA",
	"GetFileVersionInfoByHandle",
	"GetFileVersionInfoExA",
	"GetFileVersionInfoExW",
	"GetFileVersionInfoSizeA",
	"GetFileVersionInfoSizeExA",
	"GetFileVersionInfoSizeExW",
	"GetFileVersionInfoSizeW",
	"GetFileVersionInfoW",
	"VerFindFileA",
	"VerFindFileW",
	"VerInstallFileA",
	"VerInstallFileW",
	"VerLanguageNameA",
	"VerLanguageNameW",
	"VerQueryValueA",
	"VerQueryValueW"
};

namespace exports
{
	void Load();
}
