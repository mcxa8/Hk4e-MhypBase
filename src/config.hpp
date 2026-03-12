#pragma once

#include "pch.h"
#include <SimpleIni.h>

#include "util.hpp"

namespace config
{
	static CSimpleIni ini;

	bool TryExtractUserAssemblyHash(const std::string& line, std::string& hash)
	{
		const char* marker = "UserAssembly.dll";
		size_t markerPos = line.find(marker);
		if (markerPos == std::string::npos)
		{
			return false;
		}

		size_t quotePos = line.find('"', markerPos);
		if (quotePos == std::string::npos)
		{
			return false;
		}

		size_t hashPos = quotePos + 1;
		if (hashPos + 32 > line.size())
		{
			return false;
		}

		for (size_t i = 0; i < 32; ++i)
		{
			char ch = line[hashPos + i];
			bool isHex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
			if (!isHex)
			{
				return false;
			}
		}

		if (hashPos + 32 >= line.size() || line[hashPos + 32] != '"')
		{
			return false;
		}

		hash = line.substr(hashPos, 32);
		return true;
	}

	bool TryReadDetectedClientVersion(const char* pkgVersionPath, const CSimpleIni& iniRef, const char*& outVersion)
	{
		FILE* file = nullptr;
		if (fopen_s(&file, pkgVersionPath, "rb") != 0 || file == nullptr)
		{
			return false;
		}

		char lineBuffer[2048] = {};
		while (fgets(lineBuffer, sizeof(lineBuffer), file) != nullptr)
		{
			std::string line = lineBuffer;
			std::string hash;
			if (TryExtractUserAssemblyHash(line, hash))
			{
				outVersion = iniRef.GetValue("MD5ClientVersion", hash.c_str(), nullptr);
				fclose(file);
				return outVersion != nullptr;
			}
		}

		fclose(file);
		return false;
	}

	static const char* client_version;
	static const char* config_channel;
	static const char* config_base_url;
	static const char* public_rsa_key;
	static const char* rsa_encrypt_key;
	static const char* private_rsa_key;
	static long magic_a;
	static long magic_b;

	bool GetEnableValue(const char* a_pKey, bool a_nDefault)
	{
		return ini.GetBoolValue("Basic", a_pKey, a_nDefault);
	}

	long GetLongValue(const char* a_pKey, long a_nDefault)
	{
		return ini.GetLongValue("Basic", a_pKey, a_nDefault);
	}

	long GetMagicA()
	{
		return magic_a;
	}

	long GetMagicB()
	{
		return magic_b;
	}

	long GetOffsetValue(const char* a_pKey, long a_nDefault)
	{
		return ini.GetLongValue(client_version, a_pKey, a_nDefault);
	}

	uintptr_t GetAddress(uintptr_t baseAddress, const char* a_pKey, long a_nDefault)
	{
		auto offset = GetOffsetValue(a_pKey, a_nDefault);
		if (offset == 0)
		{
			auto patternKey = std::string(a_pKey) + "_Pattern";
			auto pattern = ini.GetValue("Offset", patternKey.c_str(), nullptr);
			if (pattern != nullptr && strlen(pattern) > 0)
			{
				if (*pattern == '+')
				{
					pattern = pattern + 1;
					auto value = util::FindEntry(util::PatternScan("UserAssembly.dll", pattern));
					if (value)
						offset = static_cast<long>(value - baseAddress);
				}
				else
				{
					auto value = util::PatternScan("UserAssembly.dll", pattern);
					if (value)
						offset = static_cast<long>(value - baseAddress);
				}
			}
		}
		if (offset) {
			util::Logf("[%s] %s = 0x%08X", client_version, a_pKey, offset);
		}
		else
		{
			util::Logf("[%s] Failed to resolve %s", client_version == nullptr ? "<null>" : client_version, a_pKey);
		}
		if (offset == 0)
		{
			return 0;
		}
		return baseAddress + static_cast<uintptr_t>(offset);
	}

	const char* GetConfigChannel()
	{
		return config_channel;
	}

	const char* GetConfigBaseUrl()
	{
		return config_base_url;
	}

	const char* GetPublicRSAKey()
	{
		return public_rsa_key;
	}

	const char* GetRSAEncryptKey()
	{
		return rsa_encrypt_key;
	}

	const char* GetPrivateRSAKey()
	{
		return private_rsa_key;
	}

	void Load()
	{
		ini.SetUnicode();
		auto configPath = util::GetConfigPath();
		auto loadStatus = ini.LoadFile(configPath.c_str());
		util::Logf("Loaded config file %s with status %d", configPath.c_str(), loadStatus);
		if (GetEnableValue("EnableConsole", false))
		{
			util::InitConsole();
		}
		client_version = ini.GetValue("Offset", "ClientVersion", nullptr);
		if (client_version == nullptr)
		{
			char filename[MAX_PATH] = {};
			GetModuleFileNameA(NULL, filename, MAX_PATH);
			char* lastSlash = strrchr(filename, '\\');
			if (lastSlash == nullptr)
			{
				lastSlash = strrchr(filename, '/');
			}
			if (lastSlash != nullptr)
			{
				lastSlash[1] = '\0';
			}

			char pkgVersionPath[MAX_PATH] = {};
			sprintf_s(pkgVersionPath, "%spkg_version", filename);
			if (TryReadDetectedClientVersion(pkgVersionPath, ini, client_version))
			{
				util::Logf("Version detected %s", client_version);
			}
			else
			{
				client_version = "Offset";
			}
		}
		magic_a = ini.GetLongValue(client_version, "magic_a", 0);
		magic_b = ini.GetLongValue(client_version, "magic_b", 0);
		config_channel = ini.GetValue("Value", "ConfigChannel", nullptr);
		config_base_url = ini.GetValue("Value", "ConfigBaseUrl", nullptr);
		public_rsa_key = ini.GetValue("Value", "PublicRSAKey", nullptr);
		rsa_encrypt_key = ini.GetValue("Value", "RSAEncryptKey", public_rsa_key);
		private_rsa_key = ini.GetValue("Value", "PrivateRSAKey", nullptr);
		util::Logf("Config summary: version=%s magic_a=%ld magic_b=%ld ConfigChannel=%s ConfigBaseUrl=%s PublicRSAKey=%s RSAEncryptKey=%s PrivateRSAKey=%s",
			client_version == nullptr ? "<null>" : client_version,
			magic_a,
			magic_b,
			config_channel == nullptr ? "unset" : "set",
			config_base_url == nullptr ? "unset" : "set",
			public_rsa_key == nullptr ? "unset" : "set",
			rsa_encrypt_key == nullptr ? "unset" : "set",
			private_rsa_key == nullptr ? "unset" : "set");
	}
}
