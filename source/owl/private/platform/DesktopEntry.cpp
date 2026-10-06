/**
 * @file DesktopEntry.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "platform/DesktopEntry.h"

#include "core/Environment.h"
#include "core/Log.h"

#include <format>
#include <fstream>
#include <sstream>

namespace owl::platform {

namespace {
#ifdef __linux__
[[nodiscard]] auto readEnv(const char* iName) -> std::optional<std::filesystem::path> {
	const auto value = core::getEnv(iName);
	if (value.empty())
		return std::nullopt;
	return std::filesystem::path{value};
}
#endif

[[nodiscard]] auto readFile(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream file(iPath, std::ios::binary);
	if (!file.good())
		return {};
	std::ostringstream content;
	content << file.rdbuf();
	return content.str();
}
}// namespace

auto desktopEntryText(const DesktopEntry& iEntry) -> std::string {
	return std::format("[Desktop Entry]\n"
					   "Type=Application\n"
					   "Name={}\n"
					   "Exec=\"{}\"\n"
					   "Icon={}\n"
					   "StartupWMClass={}\n"
					   "NoDisplay=true\n"
					   "Terminal=false\n"
					   "X-Owl-Generated=true\n",
					   iEntry.name, iEntry.executable.string(), iEntry.icon.string(), iEntry.appId);
}

auto desktopEntryPath(const std::string_view iAppId) -> std::optional<std::filesystem::path> {
#ifdef __linux__
	auto dataHome = readEnv("XDG_DATA_HOME");
	if (!dataHome.has_value()) {
		if (const auto home = readEnv("HOME"); home.has_value())
			dataHome = *home / ".local" / "share";
	}
	if (!dataHome.has_value())
		return std::nullopt;
	return *dataHome / "applications" / std::format("{}.desktop", iAppId);
#else
	static_cast<void>(iAppId);
	return std::nullopt;
#endif
}

auto installDesktopEntry(const DesktopEntry& iEntry) -> bool {
	if (iEntry.appId.empty() || iEntry.icon.empty()) {
		OWL_CORE_WARN("DesktopEntry: Missing application id or icon, entry not written.")
		return false;
	}
	const auto path = desktopEntryPath(iEntry.appId);
	if (!path.has_value()) {
		OWL_CORE_WARN("DesktopEntry: No user data directory for {}.", iEntry.appId)
		return false;
	}
	const auto text = desktopEntryText(iEntry);
	if (exists(*path) && readFile(*path) == text)
		return true;
	std::error_code errorCode;
	std::filesystem::create_directories(path->parent_path(), errorCode);
	std::ofstream file(*path, std::ios::binary | std::ios::trunc);
	if (!file.good()) {
		OWL_CORE_WARN("DesktopEntry: Cannot write {}.", path->string())
		return false;
	}
	file << text;
	OWL_CORE_INFO("DesktopEntry: Wrote {}.", path->string())
	return true;
}

}// namespace owl::platform
