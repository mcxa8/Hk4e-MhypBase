#include "pch.h"

#include "exports.h"

namespace
{
	HANDLE g_export_log = INVALID_HANDLE_VALUE;

	HANDLE EnsureExportLog()
	{
		if (g_export_log != INVALID_HANDLE_VALUE)
		{
			return g_export_log;
		}

		char processPath[MAX_PATH] = {};
		DWORD length = GetModuleFileNameA(nullptr, processPath, MAX_PATH);
		char logPath[MAX_PATH] = "mhypbase.log";
		if (length > 0 && length < MAX_PATH)
		{
			for (DWORD i = length; i > 0; --i)
			{
				if (processPath[i - 1] == '\\' || processPath[i - 1] == '/')
				{
					processPath[i] = '\0';
					break;
				}
			}
			sprintf_s(logPath, "%smhypbase.log", processPath);
		}

		g_export_log = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (g_export_log == INVALID_HANDLE_VALUE)
		{
			g_export_log = CreateFileA("mhypbase.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		}

		return g_export_log;
	}

	void ExportLog(const char* fmt, ...)
	{
		char buffer[1024] = {};
		va_list args;
		va_start(args, fmt);
		_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
		va_end(args);

		HANDLE log = EnsureExportLog();
		if (log != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			WriteFile(log, buffer, static_cast<DWORD>(strlen(buffer)), &written, nullptr);
			WriteFile(log, "\r\n", 2, &written, nullptr);
		}

		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
	}

	void ExportLogLastError(const char* action)
	{
		ExportLog("%s failed with Win32 error %lu", action, GetLastError());
	}
}

FARPROC OriginalFuncs_version[17] = {};

namespace exports
{
	void Load()
	{
		if (OriginalFuncs_version[0] != nullptr)
		{
			ExportLog("[exports] System version.dll exports already loaded.");
			return;
		}

		char systemDirectory[MAX_PATH] = {};
		if (GetSystemDirectoryA(systemDirectory, MAX_PATH) == 0)
		{
			ExportLogLastError("GetSystemDirectoryA");
			return;
		}

		char originalPath[MAX_PATH] = {};
		sprintf_s(originalPath, "%s\\version.dll", systemDirectory);
		ExportLog("[exports] Loading system version.dll from %s", originalPath);

		HMODULE version = LoadLibraryA(originalPath);
		if (version == nullptr)
		{
			ExportLogLastError("LoadLibraryA(system version.dll)");
			return;
		}

		FARPROC resolved[17] = {};
		for (int i = 0; i < 17; ++i)
		{
			resolved[i] = GetProcAddress(version, ExportNames_version[i]);
			if (resolved[i] == nullptr)
			{
				ExportLog("[exports] Failed to resolve system version.dll export %s", ExportNames_version[i]);
				ExportLogLastError("GetProcAddress(version export)");
				return;
			}
		}

		memcpy(OriginalFuncs_version, resolved, sizeof(resolved));
		ExportLog("[exports] Loaded system version.dll exports.");
	}
}
