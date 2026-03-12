#pragma once

#include "pch.h"

namespace util
{
	inline std::mutex log_mutex;
	inline std::ofstream log_file;
	inline HANDLE console_output = nullptr;
	inline HANDLE console_error = nullptr;
	inline HANDLE console_input = nullptr;
	inline HANDLE old_console_output = nullptr;
	inline HANDLE old_console_error = nullptr;
	inline HANDLE old_console_input = nullptr;

	inline HMODULE GetSelfModuleHandle()
	{
		MEMORY_BASIC_INFORMATION mbi;
		return ((::VirtualQuery(GetSelfModuleHandle, &mbi, sizeof(mbi)) != 0) ? (HMODULE)mbi.AllocationBase : NULL);
	}

	inline std::string GetModulePath()
	{
		char filename[MAX_PATH] = {};
		GetModuleFileNameA(GetSelfModuleHandle(), filename, MAX_PATH);
		return filename;
	}

	inline std::string GetFilePathInModuleDirectory(const char* filename)
	{
		auto path = std::filesystem::path(GetModulePath()).parent_path() / filename;
		return path.string();
	}

	inline std::string GetConfigPath()
	{
		return GetFilePathInModuleDirectory("mhypbase.ini");
	}

	inline std::string GetLogPath()
	{
		return GetFilePathInModuleDirectory("mhypbase.log");
	}

	inline std::string GetTimestamp()
	{
		SYSTEMTIME st = {};
		GetLocalTime(&st);
		char buffer[64] = {};
		sprintf_s(buffer, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
			st.wYear,
			st.wMonth,
			st.wDay,
			st.wHour,
			st.wMinute,
			st.wSecond,
			st.wMilliseconds);
		return buffer;
	}

	inline const char* GetWineVersion()
	{
		auto ntdll = GetModuleHandleA("ntdll.dll");
		if (ntdll == nullptr)
		{
			return nullptr;
		}

		using wine_get_version_t = const char* (__cdecl*)();
		auto wine_get_version = reinterpret_cast<wine_get_version_t>(GetProcAddress(ntdll, "wine_get_version"));
		if (wine_get_version == nullptr)
		{
			return nullptr;
		}

		return wine_get_version();
	}

	inline void EnsureLogFileOpenLocked()
	{
		if (!log_file.is_open())
		{
			log_file.open(GetLogPath(), std::ios::out | std::ios::app);
		}
	}

	inline void AttachConsole()
	{
		if (console_output != nullptr && console_output != INVALID_HANDLE_VALUE)
		{
			return;
		}

		old_console_output = GetStdHandle(STD_OUTPUT_HANDLE);
		old_console_error = GetStdHandle(STD_ERROR_HANDLE);
		old_console_input = GetStdHandle(STD_INPUT_HANDLE);

		::AllocConsole() && ::AttachConsole(GetCurrentProcessId());

		console_output = GetStdHandle(STD_OUTPUT_HANDLE);
		console_error = GetStdHandle(STD_ERROR_HANDLE);
		console_input = GetStdHandle(STD_INPUT_HANDLE);

		if (console_output != nullptr && console_output != INVALID_HANDLE_VALUE)
		{
			SetConsoleMode(console_output, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
		}

		if (console_input != nullptr && console_input != INVALID_HANDLE_VALUE)
		{
			SetConsoleMode(console_input,
				ENABLE_INSERT_MODE | ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);
		}
	}

	inline void DetachConsole()
	{
		if (console_output && console_error && console_input)
		{
			FreeConsole();

			if (old_console_output)
				SetStdHandle(STD_OUTPUT_HANDLE, old_console_output);
			if (old_console_error)
				SetStdHandle(STD_ERROR_HANDLE, old_console_error);
			if (old_console_input)
				SetStdHandle(STD_INPUT_HANDLE, old_console_input);
		}

		console_output = nullptr;
		console_error = nullptr;
		console_input = nullptr;
	}

	inline bool ConsolePrint(const char* fmt, ...)
	{
		if (console_output == nullptr || console_output == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		char buffer[2048] = {};
		va_list args;
		va_start(args, fmt);
		_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
		va_end(args);

		return !!WriteConsoleA(console_output, buffer, static_cast<DWORD>(strlen(buffer)), nullptr, nullptr);
	}

	inline void WriteLine(const char* level, const char* text)
	{
		std::lock_guard<std::mutex> lock(log_mutex);
		EnsureLogFileOpenLocked();

		std::ostringstream stream;
		stream << GetTimestamp() << " [pid:" << GetCurrentProcessId() << "] [tid:" << GetCurrentThreadId() << "] [" << level << "] " << (text ? text : "");
		const std::string line = stream.str();

		if (log_file.is_open())
		{
			log_file << line << std::endl;
			log_file.flush();
		}

		ConsolePrint("%s\n", line.c_str());
	}

	inline void Log(const char* text)
	{
		WriteLine("info", text);
	}

	inline void Logf(const char* fmt, ...)
	{
		char text[2048] = {};

		va_list args;
		va_start(args, fmt);
		vsprintf_s(text, fmt, args);
		va_end(args);

		Log(text);
	}

	inline void LogLastError(const char* action)
	{
		const DWORD error = GetLastError();
		LPSTR buffer = nullptr;
		const DWORD messageLength = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&buffer),
			0,
			nullptr);

		if (messageLength == 0 || buffer == nullptr)
		{
			Logf("%s failed with Win32 error %lu.", action, error);
			return;
		}

		DWORD trimmedLength = messageLength;
		while (trimmedLength > 0 && (buffer[trimmedLength - 1] == '\r' || buffer[trimmedLength - 1] == '\n'))
		{
			buffer[trimmedLength - 1] = '\0';
			--trimmedLength;
		}

		Logf("%s failed with Win32 error %lu: %s", action, error, buffer);
		LocalFree(buffer);
	}

	inline void Flogf(const char* fmt, ...)
	{
		char text[2048] = {};

		va_list args;
		va_start(args, fmt);
		vsprintf_s(text, fmt, args);
		va_end(args);

		WriteLine("dump", text);
	}

	inline std::string ConvertToString(VOID* ptr)
	{
		if (ptr == nullptr)
		{
			return {};
		}

		auto bytePtr = reinterpret_cast<unsigned char*>(ptr);
		auto lengthPtr = reinterpret_cast<unsigned int*>(bytePtr + 0x10);
		auto charPtr = reinterpret_cast<char16_t*>(bytePtr + 0x14);
		auto size = lengthPtr[0];
		std::u16string u16;
		u16.resize(size);
		memcpy((char*)&u16[0], (char*)charPtr, size * sizeof(char16_t));
		std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
		return converter.to_bytes(u16);
	}

	inline void InitConsole()
	{
		AttachConsole();
		Log("Console initialized.");
	}

	inline void DisableLogReport()
	{
		char szProcessPath[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, szProcessPath, MAX_PATH);

		auto path = std::filesystem::path(szProcessPath);
		auto processName = path.filename().string();
		processName = processName.substr(0, processName.find_last_of('.'));

		auto astrolabe = path.parent_path() / (processName + "_Data\\Plugins\\Astrolabe.dll");
		auto miHoYoMTRSDK = path.parent_path() / (processName + "_Data\\Plugins\\MiHoYoMTRSDK.dll");
		Logf("Trying to lock log report modules: %s ; %s", astrolabe.string().c_str(), miHoYoMTRSDK.string().c_str());

		HANDLE astrolabeHandle = CreateFileA(astrolabe.string().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (astrolabeHandle == INVALID_HANDLE_VALUE)
		{
			LogLastError("CreateFileA(Astrolabe.dll)");
		}

		HANDLE sdkHandle = CreateFileA(miHoYoMTRSDK.string().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (sdkHandle == INVALID_HANDLE_VALUE)
		{
			LogLastError("CreateFileA(MiHoYoMTRSDK.dll)");
		}
	}

	inline void DisableVMProtect()
	{
		auto ntdll = GetModuleHandleA("ntdll.dll");
		if (ntdll == nullptr)
		{
			Log("ntdll.dll is not loaded, skipping VMProtect patch.");
			return;
		}

		auto ntProtectVirtualMemory = reinterpret_cast<std::uint8_t*>(GetProcAddress(ntdll, "NtProtectVirtualMemory"));
		auto ntQuerySection = reinterpret_cast<std::uint8_t*>(GetProcAddress(ntdll, "NtQuerySection"));
		auto ntPulseEvent = reinterpret_cast<std::uint8_t*>(GetProcAddress(ntdll, "NtPulseEvent"));
		const bool isWine = GetWineVersion() != nullptr;
		auto sourceRoutine = isWine ? ntPulseEvent : ntQuerySection;

		if (ntProtectVirtualMemory == nullptr || sourceRoutine == nullptr)
		{
			Logf("Unable to resolve VMProtect patch routines. NtProtectVirtualMemory=%p NtQuerySection=%p NtPulseEvent=%p",
				ntProtectVirtualMemory,
				ntQuerySection,
				ntPulseEvent);
			return;
		}

		DWORD oldProtect = 0;
		if (!VirtualProtect(ntProtectVirtualMemory, sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			LogLastError("VirtualProtect(NtProtectVirtualMemory)");
			return;
		}

		uintptr_t patchedStub = *reinterpret_cast<uintptr_t*>(sourceRoutine);
		patchedStub &= ~(0xFFull << 32);
		patchedStub |= static_cast<uintptr_t>(*reinterpret_cast<std::uint32_t*>(sourceRoutine + 4) - 1) << 32;
		*reinterpret_cast<uintptr_t*>(ntProtectVirtualMemory) = patchedStub;

		DWORD restoredProtect = 0;
		if (!VirtualProtect(ntProtectVirtualMemory, sizeof(uintptr_t), oldProtect, &restoredProtect))
		{
			LogLastError("VirtualProtect restore(NtProtectVirtualMemory)");
		}

		Logf("Patched NtProtectVirtualMemory using %s stub at %p.", isWine ? "NtPulseEvent/Wine" : "NtQuerySection/Windows", sourceRoutine);
	}

	// https://github.com/34736384/RSAPatch/blob/master/RSAPatch/Utils.cpp
	inline uintptr_t FindEntry(uintptr_t addr)
	{
		__try
		{
			while (true)
			{
				// walk back until we find function entry
				uint32_t code = *(uint32_t*)addr;
				code &= ~0xFF000000;
				if (_byteswap_ulong(code) == 0x4883EC00) // sub rsp, ??
					return addr;
				addr--;
			}
		}
		__except (1)
		{
		}
		return 0;
	}

	// https://github.com/34736384/RSAPatch/blob/master/RSAPatch/Utils.cpp
	inline uintptr_t PatternScan(LPCSTR module, LPCSTR pattern)
	{
		static auto pattern_to_byte = [](const char* pattern)
		{
			auto bytes = std::vector<int>{};
			auto start = const_cast<char*>(pattern);
			auto end = const_cast<char*>(pattern) + strlen(pattern);
			for (auto current = start; current < end; ++current)
			{
				if (*current == '?')
				{
					++current;
					if (*current == '?')
						++current;
					bytes.push_back(-1);
				}
				else
				{
					bytes.push_back(strtoul(current, &current, 16));
				}
			}
			return bytes;
		};

		auto mod = GetModuleHandle(module);
		if (!mod)
			return 0;

		auto dosHeader = (PIMAGE_DOS_HEADER)mod;
		auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)mod + dosHeader->e_lfanew);
		auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
		auto patternBytes = pattern_to_byte(pattern);
		auto scanBytes = reinterpret_cast<std::uint8_t*>(mod);
		auto s = patternBytes.size();
		auto d = patternBytes.data();

		for (auto i = 0ul; i < sizeOfImage - s; ++i)
		{
			bool found = true;
			for (auto j = 0ul; j < s; ++j)
			{
				if (scanBytes[i + j] != d[j] && d[j] != -1)
				{
					found = false;
					break;
				}
			}

			if (found)
			{
				return (uintptr_t)&scanBytes[i];
			}
		}
		return 0;
	}

	inline void DumpAddress(uint32_t start, long magic_a, long magic_b)
	{
		uintptr_t baseAddress = (uintptr_t)GetModuleHandle("UserAssembly.dll");
		for (uint32_t i = start; ; i++)
		{
			auto klass = il2cpp__vm__MetadataCache__GetTypeInfoFromTypeDefinitionIndex(i);
			// &reinterpret_cast<uintptr_t*>(klass)[?] is a magic for klass->byval_arg
			std::string class_name = il2cpp__vm__Type__GetName(&reinterpret_cast<uintptr_t*>(klass)[magic_a], 0);
			util::Flogf("[%d]\n%s", i, class_name.c_str());
			void* iter = 0;
			while (const LPVOID method = il2cpp__vm__Class__GetMethods(klass, (LPVOID)&iter))
			{
				// &reinterpret_cast<uintptr_t*>(method)[?] is a magic for method->methodPointer
				auto method_address = reinterpret_cast<uintptr_t*>(method)[magic_b];
				if (method_address)
					method_address -= baseAddress;
				std::string method_name = il2cpp__vm__Method__GetNameWithGenericTypes(method);
				util::Flogf("\t0x%08X: %s", method_address, method_name.c_str());
			}
			util::Flogf("");
		}
	}
}
