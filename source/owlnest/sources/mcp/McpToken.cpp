/**
 * @file McpToken.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "McpToken.h"

#include <core/Core.h>
#include <core/Environment.h>
#include <core/Log.h>

#include <array>
#include <cstdint>
#include <system_error>
#include <vector>

#ifdef OWL_PLATFORM_WINDOWS
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
#include <fstream>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace owl::nest::mcp {

namespace {

auto fillRandom(std::vector<uint8_t>& oBytes) -> bool {
#ifdef OWL_PLATFORM_WINDOWS
	return BCryptGenRandom(nullptr, oBytes.data(), static_cast<ULONG>(oBytes.size()),
						   BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
	size_t filled = 0;
	while (filled < oBytes.size()) {
		const auto got = getrandom(oBytes.data() + filled, oBytes.size() - filled, 0);
		if (got < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		filled += static_cast<size_t>(got);
	}
	return true;
#endif
}

auto getConfigBase() -> std::filesystem::path {
#ifdef OWL_PLATFORM_WINDOWS
	if (auto local = core::getEnv("LOCALAPPDATA"); !local.empty())
		return local;
	if (auto roaming = core::getEnv("APPDATA"); !roaming.empty())
		return roaming;
#else
	if (auto runtime = core::getEnv("XDG_RUNTIME_DIR"); !runtime.empty())
		return runtime;
	if (auto config = core::getEnv("XDG_CONFIG_HOME"); !config.empty())
		return config;
	if (const auto home = core::getEnv("HOME"); !home.empty())
		return std::filesystem::path{home} / ".config";
#endif
	return std::filesystem::temp_directory_path();
}

auto createPrivateDirectory(const std::filesystem::path& iDirectory) -> bool {
	std::error_code error;
	if (exists(iDirectory, error))
		return is_directory(iDirectory, error);
	if (!create_directories(iDirectory, error)) {
		OWL_CORE_WARN("Mcp: Cannot create {} ({}).", iDirectory.string(), error.message())
		return false;
	}
	permissions(iDirectory, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, error);
	return true;
}

}// namespace

auto generateRandomHex(const size_t iBytes) -> std::optional<std::string> {
	std::vector<uint8_t> bytes(iBytes);
	if (!fillRandom(bytes)) {
		OWL_CORE_ERROR("Mcp: The system random generator failed.")
		return std::nullopt;
	}
	constexpr std::array<char, 16> digits{'0', '1', '2', '3', '4', '5', '6', '7',
										  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	std::string hex;
	hex.reserve(iBytes * 2);
	for (const auto byte: bytes) {
		hex.push_back(digits[static_cast<size_t>(byte >> 4u)]);
		hex.push_back(digits[static_cast<size_t>(byte & 0x0fu)]);
	}
	return hex;
}

auto generateToken() -> std::optional<std::string> { return generateRandomHex(g_tokenBytes); }

auto isSameToken(const std::string_view iExpected, const std::string_view iCandidate) -> bool {
	if (iExpected.empty() || iExpected.size() != iCandidate.size())
		return false;
	uint8_t difference = 0;
	for (size_t i = 0; i < iExpected.size(); ++i)
		difference |= static_cast<uint8_t>(static_cast<uint8_t>(iExpected[i]) ^ static_cast<uint8_t>(iCandidate[i]));
	return difference == 0;
}

auto getDefaultTokenFile() -> std::filesystem::path { return getConfigBase() / "owlnest" / "mcp_token"; }

auto writeTokenFile(const std::filesystem::path& iFile, const std::string_view iToken) -> bool {
	if (iFile.has_parent_path() && !createPrivateDirectory(iFile.parent_path()))
		return false;
#ifdef OWL_PLATFORM_WINDOWS
	{
		std::ofstream out(iFile, std::ios::binary | std::ios::trunc);
		if (!out) {
			OWL_CORE_WARN("Mcp: Cannot write the token file {}.", iFile.string())
			return false;
		}
		out.write(iToken.data(), static_cast<std::streamsize>(iToken.size()));
	}
	std::error_code error;
	permissions(iFile, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
				std::filesystem::perm_options::replace, error);
	return true;
#else
	const int file = open(iFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (file < 0) {
		OWL_CORE_WARN("Mcp: Cannot write the token file {}.", iFile.string())
		return false;
	}
	// The file may predate this run with wider rights: open does not change them.
	bool written = fchmod(file, S_IRUSR | S_IWUSR) == 0;
	size_t done = 0;
	while (written && done < iToken.size()) {
		const auto count = write(file, iToken.data() + done, iToken.size() - done);
		if (count < 0 && errno == EINTR)
			continue;
		written = count > 0;
		if (written)
			done += static_cast<size_t>(count);
	}
	written = close(file) == 0 && written;
	if (!written)
		OWL_CORE_WARN("Mcp: Cannot write the token file {}.", iFile.string())
	return written;
#endif
}

void removeTokenFile(const std::filesystem::path& iFile) {
	std::error_code error;
	std::filesystem::remove(iFile, error);
}

}// namespace owl::nest::mcp
