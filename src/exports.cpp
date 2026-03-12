#include "pch.h"

#include "exports.h"
#include "util.hpp"

#include <string>

FARPROC OriginalFuncs_version[17] = {};

namespace exports
{
	void Load()
	{
		if (OriginalFuncs_version[0] != nullptr)
		{
			util::Log("System version.dll exports already loaded.");
			return;
		}

		char systemDirectory[MAX_PATH] = {};
		if (GetSystemDirectoryA(systemDirectory, MAX_PATH) == 0)
		{
			util::LogLastError("GetSystemDirectoryA");
			return;
		}

		std::string originalPath = systemDirectory;
		originalPath += "\\version.dll";
		util::Logf("Loading system version.dll from %s", originalPath.c_str());

		HMODULE version = LoadLibraryA(originalPath.c_str());
		if (version == nullptr)
		{
			util::LogLastError("LoadLibraryA(system version.dll)");
			return;
		}

		FARPROC resolved[17] = {};
		for (int i = 0; i < 17; ++i)
		{
			resolved[i] = GetProcAddress(version, ExportNames_version[i]);
			if (resolved[i] == nullptr)
			{
				util::Logf("Failed to resolve system version.dll export %s", ExportNames_version[i]);
				util::LogLastError("GetProcAddress(version export)");
				return;
			}
		}

		memcpy(OriginalFuncs_version, resolved, sizeof(resolved));
		util::Log("Loaded system version.dll exports.");
	}
}
