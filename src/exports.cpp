#include "pch.h"

#include "exports.h"

#include <string>

namespace
{
	HMODULE GetSelfModuleHandle()
	{
		MEMORY_BASIC_INFORMATION mbi = {};
		return (::VirtualQuery(GetSelfModuleHandle, &mbi, sizeof(mbi)) != 0) ? static_cast<HMODULE>(mbi.AllocationBase) : nullptr;
	}

	std::string GetLogPath()
	{
		char filename[MAX_PATH] = {};
		GetModuleFileNameA(GetSelfModuleHandle(), filename, MAX_PATH);
		return (std::filesystem::path(filename).parent_path() / "mhypbase.log").string();
	}

	void LogExportMessage(const char* message)
	{
		std::ofstream output(GetLogPath(), std::ios::out | std::ios::app);
		if (!output.is_open())
		{
			return;
		}

		output << "[exports] " << message << std::endl;
	}

	void LogExportError(const char* action)
	{
		const DWORD error = GetLastError();
		char text[512] = {};
		sprintf_s(text, "%s failed with Win32 error %lu", action, error);
		LogExportMessage(text);
	}
}

FARPROC OriginalFuncs_version[17] = {};

namespace exports
{
	void Load()
	{
		if (OriginalFuncs_version[0] != nullptr)
		{
			LogExportMessage("System version.dll exports already loaded.");
			return;
		}

		char systemDirectory[MAX_PATH] = {};
		if (GetSystemDirectoryA(systemDirectory, MAX_PATH) == 0)
		{
			LogExportError("GetSystemDirectoryA");
			return;
		}

		std::string originalPath = systemDirectory;
		originalPath += "\\version.dll";
		{
			char text[512] = {};
			sprintf_s(text, "Loading system version.dll from %s", originalPath.c_str());
			LogExportMessage(text);
		}

		HMODULE version = LoadLibraryA(originalPath.c_str());
		if (version == nullptr)
		{
			LogExportError("LoadLibraryA(system version.dll)");
			return;
		}

		FARPROC resolved[17] = {};
		for (int i = 0; i < 17; ++i)
		{
			resolved[i] = GetProcAddress(version, ExportNames_version[i]);
			if (resolved[i] == nullptr)
			{
				char text[256] = {};
				sprintf_s(text, "Failed to resolve system version.dll export %s", ExportNames_version[i]);
				LogExportMessage(text);
				LogExportError("GetProcAddress(version export)");
				return;
			}
		}

		memcpy(OriginalFuncs_version, resolved, sizeof(resolved));
		LogExportMessage("Loaded system version.dll exports.");
	}
}
