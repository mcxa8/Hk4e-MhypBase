#include "pch.h"

#include "exports.h"
#include "il2cpp-init.hpp"

#include "config.hpp"
#include "hook.hpp"
#include "util.hpp"

namespace
{
	struct UnityWindowState
	{
		DWORD pid;
		HWND hwnd;
	};

	BOOL CALLBACK FindUnityWindow(HWND hwnd, LPARAM lParam)
	{
		auto state = reinterpret_cast<UnityWindowState*>(lParam);
		DWORD windowPid = 0;
		GetWindowThreadProcessId(hwnd, &windowPid);
		if (windowPid != state->pid)
		{
			return TRUE;
		}

		char windowClass[256] = {};
		if (GetClassNameA(hwnd, windowClass, sizeof(windowClass)) == 0)
		{
			return TRUE;
		}

		if (strcmp(windowClass, "UnityWndClass") == 0)
		{
			state->hwnd = hwnd;
			return FALSE;
		}

		return TRUE;
	}

	void WaitForUnityWindow()
	{
		UnityWindowState state = { GetCurrentProcessId(), nullptr };
		for (int attempt = 1; state.hwnd == nullptr; ++attempt)
		{
			EnumWindows(FindUnityWindow, reinterpret_cast<LPARAM>(&state));
			if (state.hwnd != nullptr)
			{
				util::Logf("Detected Unity window %p after %d attempts.", state.hwnd, attempt);
				return;
			}

			util::Logf("Unity window not ready yet (attempt %d).", attempt);
			Sleep(1000);
		}
	}
}

DWORD WINAPI Thread(LPVOID lpParam)
{
	util::Log("Worker thread started.");
	util::Logf("Worker thread parameter=%p", lpParam);
	const char* wineVersion = util::GetWineVersion();
	util::Logf("Runtime environment: %s", wineVersion == nullptr ? "native Windows" : wineVersion);

	config::Load();
	util::DisableLogReport();
	util::Log("Disabled log report.");

	while (GetModuleHandleA("UserAssembly.dll") == nullptr)
	{
		util::Log("UserAssembly.dll isn't loaded, waiting for a sec.");
		Sleep(1000);
	}
	util::Logf("UserAssembly.dll loaded at %p", GetModuleHandleA("UserAssembly.dll"));
	WaitForUnityWindow();
	util::Log("Waiting 2 sec for game initialize after Unity window detection.");
	Sleep(2000);
	util::DisableVMProtect();
	util::Log("Disabled vm protect.");

	init_il2cpp();
	util::Log("Loaded il2cpp functions.");

	hook::Load();
	util::Log("Loaded hooks.");
	util::Log("Worker thread finished initialization.");
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hinstDLL);
		char selfPath[MAX_PATH] = {};
		GetModuleFileNameA(hinstDLL, selfPath, MAX_PATH);
		util::Logf("DllMain attach: module=%s reserved=%p", selfPath, lpvReserved);
		exports::Load();
		if (HANDLE hThread = CreateThread(NULL, 0, Thread, hinstDLL, 0, NULL))
		{
			util::Logf("Created worker thread handle=%p", hThread);
			CloseHandle(hThread);
		}
		else
		{
			util::LogLastError("CreateThread");
		}
	}
	return TRUE;
}
