/**
 * @file UserDirectory.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "platform/UserDirectory.h"

#include "core/Environment.h"
#include "core/Log.h"

#ifdef OWL_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <array>
#include <fstream>
#include <mutex>
#include <optional>
#include <system_error>

namespace owl::platform {

namespace {

auto getExecutablePath() -> std::optional<std::filesystem::path> {
#ifdef OWL_PLATFORM_WINDOWS
	std::array<wchar_t, 32768> buffer{};
	const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
	if (length == 0 || length >= buffer.size())
		return std::nullopt;
	return std::filesystem::path{std::wstring{buffer.data(), length}};
#else
	std::error_code error;
	auto path = std::filesystem::canonical("/proc/self/exe", error);
	if (error)
		return std::nullopt;
	return path;
#endif
}

auto isWritableDirectory(const std::filesystem::path& iDirectory) -> bool {
	std::error_code error;
	std::filesystem::create_directories(iDirectory, error);
	if (!std::filesystem::is_directory(iDirectory, error))
		return false;
	const auto probe = iDirectory / ".owl_write_probe";
	if (!std::ofstream(probe, std::ios::binary | std::ios::trunc).good())
		return false;
	std::filesystem::remove(probe, error);
	return true;
}

auto getProfileDirectory() -> std::filesystem::path {
#ifdef OWL_PLATFORM_WINDOWS
	for (const auto* variable: {"LOCALAPPDATA", "APPDATA"}) {
		if (auto value = core::getEnv(variable); !value.empty())
			return value;
	}
#else
	if (auto dataHome = core::getEnv("XDG_DATA_HOME"); !dataHome.empty())
		return dataHome;
	if (const auto home = core::getEnv("HOME"); !home.empty())
		return std::filesystem::path{home} / ".local" / "share";
#endif
	return std::filesystem::temp_directory_path();
}

struct UserDirectoryState {
	std::mutex mutex;
	std::filesystem::path forced;
	std::filesystem::path resolved;
};

auto getState() -> UserDirectoryState& {
	static UserDirectoryState state;
	return state;
}

}// namespace

auto getExecutableDirectory() -> std::filesystem::path {
	if (const auto path = getExecutablePath(); path.has_value())
		return path->parent_path();
	return std::filesystem::current_path();
}

auto resolveUserDirectory(const std::filesystem::path& iAppDirectory, const std::string& iFallbackName)
		-> std::filesystem::path {
	if (const auto local = iAppDirectory / "user"; isWritableDirectory(local))
		return local;
	auto fallback = getProfileDirectory() / (iFallbackName.empty() ? std::string{"Owl"} : iFallbackName);
	std::error_code error;
	std::filesystem::create_directories(fallback, error);
	if (error)
		OWL_CORE_WARN("UserDirectory: Cannot create '{}': {}.", fallback.string(), error.message())
	OWL_CORE_INFO("UserDirectory: '{}' is read-only, using '{}'.", iAppDirectory.string(), fallback.string())
	return fallback;
}

auto getUserDirectory() -> std::filesystem::path {
	auto& state = getState();
	const std::scoped_lock<std::mutex> lock(state.mutex);
	if (!state.forced.empty())
		return state.forced;
	if (state.resolved.empty()) {
		if (const auto fromEnvironment = core::getEnv("OWL_USER_DIR"); !fromEnvironment.empty()) {
			state.resolved = std::filesystem::absolute(fromEnvironment);
			std::error_code error;
			std::filesystem::create_directories(state.resolved, error);
		} else {
			const auto executable = getExecutablePath();
			const auto name = executable.has_value() ? executable->stem().string() : std::string{};
			state.resolved = resolveUserDirectory(getExecutableDirectory(), name);
		}
	}
	return state.resolved;
}

void setUserDirectory(const std::filesystem::path& iDirectory) {
	auto& state = getState();
	const std::scoped_lock<std::mutex> lock(state.mutex);
	state.forced = iDirectory.empty() ? std::filesystem::path{} : std::filesystem::absolute(iDirectory);
	if (state.forced.empty())
		return;
	std::error_code error;
	std::filesystem::create_directories(state.forced, error);
	if (error)
		OWL_CORE_WARN("UserDirectory: Cannot create '{}': {}.", state.forced.string(), error.message())
}

auto getGameDirectory(const std::string& iGameName) -> std::filesystem::path {
	return getUserDirectory() / "games" / (iGameName.empty() ? std::string{"OwlGame"} : iGameName);
}

}// namespace owl::platform
