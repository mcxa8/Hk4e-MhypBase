#include "pch.h"
#include "il2cpp-init.hpp"

#include "config.hpp"
#include "hook.hpp"
#include "util.hpp"

DWORD WINAPI Thread(LPVOID lpParam)
{
	config::Load();
	util::DisableLogReport();
	util::Log("Disabled log report.");

	while (GetModuleHandle("UserAssembly.dll") == nullptr)
	{
		util::Log("UserAssembly.dll isn't loaded, waiting for a sec.");
		Sleep(1000);
	}
	DWORD pid = GetCurrentProcessId();
	while (true)
	{
		DWORD currentPid = pid;
		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL __stdcall
		{
			DWORD windowPid = 0;
			GetWindowThreadProcessId(hwnd, &windowPid);

			char windowClass[256] = {};
			GetClassNameA(hwnd, windowClass, sizeof(windowClass));
			if (windowPid == *reinterpret_cast<DWORD*>(lParam) && strcmp(windowClass, "UnityWndClass") == 0)
			{
				*reinterpret_cast<DWORD*>(lParam) = 0;
				return FALSE;
			}
			return TRUE;
		}, reinterpret_cast<LPARAM>(&currentPid));

		if (currentPid == 0)
		{
			break;
		}

		util::Log("Unity window isn't ready, waiting for a sec.");
		Sleep(1000);
	}
	util::DisableVMProtect();
	util::Log("Disabled vm protect.");

	init_il2cpp();
	util::Log("Loaded il2cpp functions.");

	hook::Load();
	util::Log("Loaded hooks.");
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		if (HANDLE hThread = CreateThread(NULL, 0, Thread, NULL, 0, NULL))
			CloseHandle(hThread);
	return TRUE;
}
