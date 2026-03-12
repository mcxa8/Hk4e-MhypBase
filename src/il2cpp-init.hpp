#pragma once

#include "pch.h"
#include "il2cpp-appdata.h"

#include "config.hpp"

#define DO_API(a, r, n, p) r(*n) p
#include "il2cpp-api-functions.h"
#undef DO_API

#define DO_APP_FUNC(a, r, n, p) r(*n) p
namespace app
{
#include "il2cpp-functions.h"
}
#undef DO_APP_FUNC

VOID init_il2cpp()
{
	uintptr_t baseAddress = (UINT64)GetModuleHandle("UserAssembly.dll");
	util::Logf("Initializing il2cpp with UserAssembly.dll base=%p", reinterpret_cast<void*>(baseAddress));

#define DO_API(a, r, n, p) n = (r(*) p)(config::GetAddress(baseAddress, #n, a))
#include "il2cpp-api-functions.h"
#undef DO_API

#define DO_APP_FUNC(a, r, n, p) n = (r(*) p)(config::GetAddress(baseAddress, #n, a))
#include "il2cpp-functions.h"
#undef DO_APP_FUNC
	util::Log("Finished resolving il2cpp exports and app functions.");
}
