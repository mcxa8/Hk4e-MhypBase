#pragma once

#include "pch.h"

namespace util
{
	void Log(const char* text)
	{
		std::cout << "[mhypbase] " << text << std::endl;
	}

	void Logf(const char* fmt, ...)
	{
		char text[1024];

		va_list args;
		va_start(args, fmt);
		vsprintf_s(text, fmt, args);
		va_end(args);

		Log(text);
	}

	std::ofstream fout;

	void Flogf(const char* fmt, ...)
	{
		if (!fout.is_open())
			fout.open("mhypbase.log");

		char text[1024];

		va_list args;
		va_start(args, fmt);
		vsprintf_s(text, fmt, args);
		va_end(args);

		fout << text << std::endl;
		fout.flush();
	}

	HMODULE GetSelfModuleHandle()
	{
		MEMORY_BASIC_INFORMATION mbi;
		return ((::VirtualQuery(GetSelfModuleHandle, &mbi, sizeof(mbi)) != 0) ? (HMODULE)mbi.AllocationBase : NULL);
	}

	std::string GetConfigPath()
	{
		char filename[MAX_PATH] = {};
		GetModuleFileName(GetSelfModuleHandle(), filename, MAX_PATH);
		auto path = std::filesystem::path(filename).parent_path() / "mhypbase.ini";
		return path.string();
	}

	std::string ConvertToString(VOID* ptr)
	{
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

	void InitConsole()
	{
		AllocConsole();

		freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
		freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

		auto consoleWindow = GetConsoleWindow();
		SetForegroundWindow(consoleWindow);
		ShowWindow(consoleWindow, SW_RESTORE);
		ShowWindow(consoleWindow, SW_SHOW);
	}

	void DisableLogReport()
	{
		char filename[MAX_PATH] = {};
		GetModuleFileName(NULL, filename, MAX_PATH);

		auto path = std::filesystem::path(filename);
		path = path.parent_path() / (path.stem().string() + "_Data") / "Plugins";

		CreateFileW((path / "Astrolabe.dll").c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		CreateFileW((path / "MiHoYoMTRSDK.dll").c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	// https://github.com/yubie-re/vmp-virtualprotect-bypass/blob/main/src/vp-patch.hpp
	void DisableVMProtect()
	{
		auto ntdll = GetModuleHandleA("ntdll.dll");
		if (ntdll == nullptr)
		{
			Log("Failed to get ntdll.dll handle.");
			return;
		}

		auto nt_vp = reinterpret_cast<BYTE*>(GetProcAddress(ntdll, "NtProtectVirtualMemory"));
		bool wine = GetProcAddress(ntdll, "wine_get_version") != nullptr;
		auto routine = reinterpret_cast<BYTE*>(GetProcAddress(ntdll, wine ? "NtPulseEvent" : "NtQuerySection"));
		if (nt_vp == nullptr || routine == nullptr)
		{
			Log("Failed to resolve ntdll exports for VMProtect bypass.");
			return;
		}

		DWORD old_protect = 0;
		VirtualProtect(nt_vp, sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &old_protect);
		*reinterpret_cast<uintptr_t*>(nt_vp) = *reinterpret_cast<uintptr_t*>(routine) & ~(0xFFui64 << 32)
			| (static_cast<uintptr_t>(*reinterpret_cast<uint32_t*>(routine + 4) - 1) << 32);
		VirtualProtect(nt_vp, sizeof(uintptr_t), old_protect, &old_protect);
	}

	// https://github.com/34736384/RSAPatch/blob/master/RSAPatch/Utils.cpp
	uintptr_t FindEntry(uintptr_t addr)
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
	uintptr_t PatternScan(LPCSTR module, LPCSTR pattern)
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

	void DumpAddress(uint32_t start, long magic_a, long magic_b)
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
