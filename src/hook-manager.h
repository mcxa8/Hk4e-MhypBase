#pragma once

#include "pch.h"
#include "util.hpp"
#include <map>

#define CALL_ORIGIN(function, ...) HookManager::call(function, __func__, __VA_ARGS__)

class HookManager
{
public:
	template <typename Fn>
	static void install(Fn func, Fn handler)
	{
		enable(func, handler);
		holderMap[reinterpret_cast<void*>(handler)] = reinterpret_cast<void*>(func);
	}
	template <typename Fn>
	static Fn getOrigin(Fn handler, const char* callerName = nullptr) noexcept
	{
		if (holderMap.count(reinterpret_cast<void*>(handler)) == 0)
		{
			util::Logf("Origin not found for handler: %s. Maybe racing bug.", callerName == nullptr ? "<unknown>" : callerName);
			return nullptr;
		}
		return reinterpret_cast<Fn>(holderMap[reinterpret_cast<void*>(handler)]);
	}
	template <typename Fn>
	static void detach(Fn handler) noexcept
	{
		disable(handler);
		holderMap.erase(reinterpret_cast<void*>(handler));
	}
	template <typename RType, typename... Params>
	static RType call(RType(*handler)(Params...), const char* callerName = nullptr, Params... params)
	{
		auto origin = getOrigin(handler, callerName);
		if (origin != nullptr)
			return origin(params...);
		return RType();
	}
	static void detachAll() noexcept
	{
		for (const auto& [key, value] : holderMap)
		{
			disable(key);
		}
		holderMap.clear();
	}

private:
	inline static std::map<void*, void*> holderMap{};
	template <typename Fn>
	static void disable(Fn handler)
	{
		Fn origin = getOrigin(handler);
		const LONG beginStatus = DetourTransactionBegin();
		const LONG updateStatus = DetourUpdateThread(GetCurrentThread());
		const LONG detachStatus = DetourDetach(&(PVOID&)origin, handler);
		const LONG commitStatus = DetourTransactionCommit();
		util::Logf("Detour detach target=%p handler=%p begin=%ld update=%ld detach=%ld commit=%ld",
			reinterpret_cast<void*>(origin),
			reinterpret_cast<void*>(handler),
			beginStatus,
			updateStatus,
			detachStatus,
			commitStatus);
	}
	template <typename Fn>
	static void enable(Fn& func, Fn handler)
	{
		util::Logf("Installing hook target=%p handler=%p",
			reinterpret_cast<void*>(func),
			reinterpret_cast<void*>(handler));
		const LONG beginStatus = DetourTransactionBegin();
		const LONG updateStatus = DetourUpdateThread(GetCurrentThread());
		const LONG attachStatus = DetourAttach(&(PVOID&)func, handler);
		const LONG commitStatus = DetourTransactionCommit();
		util::Logf("Detour attach target=%p handler=%p begin=%ld update=%ld attach=%ld commit=%ld",
			reinterpret_cast<void*>(func),
			reinterpret_cast<void*>(handler),
			beginStatus,
			updateStatus,
			attachStatus,
			commitStatus);
	}
};
