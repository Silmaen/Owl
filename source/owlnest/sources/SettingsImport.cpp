/**
 * @file SettingsImport.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SettingsImport.h"

#include <core/Log.h>

#include <string>
#include <system_error>
#include <vector>

namespace owl::nest::settings_import {

namespace {

constexpr auto g_pendingFolder = "import_pending";

auto hasSettings(const std::filesystem::path& iFolder) -> bool {
	std::error_code error;
	return exists(iFolder / "OwlNest_settings.yml", error) || exists(iFolder / "config.yml", error);
}

auto listImportedEntries(const std::filesystem::path& iSource) -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> entries;
	std::error_code error;
	for (const auto* name: {"config.yml", "OwlNest_settings.yml"}) {
		if (is_regular_file(iSource / name, error))
			entries.emplace_back(name);
	}
	for (const auto& entry: std::filesystem::directory_iterator(iSource, error)) {
		if (entry.is_regular_file(error) && entry.path().filename().string().ends_with("_imgui.ini"))
			entries.push_back(entry.path().filename());
	}
	if (is_directory(iSource / "games", error))
		entries.emplace_back("games");
	return entries;
}

auto countFiles(const std::filesystem::path& iPath) -> uint32_t {
	std::error_code error;
	if (!is_directory(iPath, error))
		return 1;
	uint32_t count = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(iPath, error)) {
		if (entry.is_regular_file(error))
			++count;
	}
	return count;
}

}// namespace

auto findSettingsSource(const std::filesystem::path& iFolder) -> std::optional<std::filesystem::path> {
	if (hasSettings(iFolder / "user"))
		return iFolder / "user";
	if (hasSettings(iFolder))
		return iFolder;
	return std::nullopt;
}

auto stageImport(const std::filesystem::path& iFolder, const std::filesystem::path& iUserDirectory)
		-> expected<uint32_t, ImportError> {
	const auto source = findSettingsSource(iFolder);
	if (!source.has_value()) {
		OWL_WARN("Settings import: No Owl Nest settings in '{}'.", iFolder.string())
		return unexpected{ImportError::NoSettings};
	}
	std::error_code error;
	if (std::filesystem::equivalent(*source, iUserDirectory, error)) {
		OWL_WARN("Settings import: '{}' is the current user directory.", source->string())
		return unexpected{ImportError::SameDirectory};
	}
	const auto staging = iUserDirectory / g_pendingFolder;
	std::filesystem::remove_all(staging, error);
	std::filesystem::create_directories(staging, error);
	uint32_t count = 0;
	for (const auto& entry: listImportedEntries(*source)) {
		std::filesystem::copy(
				*source / entry, staging / entry,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, error);
		if (error) {
			OWL_WARN("Settings import: Cannot copy '{}': {}.", (*source / entry).string(), error.message())
			std::filesystem::remove_all(staging, error);
			return unexpected{ImportError::CopyFailed};
		}
		count += countFiles(staging / entry);
	}
	OWL_INFO("Settings import: {} file(s) from '{}' applied at the next start.", count, source->string())
	return count;
}

auto applyPendingImport(const std::filesystem::path& iUserDirectory) -> uint32_t {
	const auto staging = iUserDirectory / g_pendingFolder;
	std::error_code error;
	if (!is_directory(staging, error))
		return 0;
	uint32_t count = 0;
	for (const auto& entry: std::filesystem::directory_iterator(staging, error)) {
		const auto target = iUserDirectory / entry.path().filename();
		count += countFiles(entry.path());
		std::filesystem::copy(
				entry.path(), target,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, error);
		if (error)
			OWL_WARN("Settings import: Cannot apply '{}': {}.", target.string(), error.message())
	}
	std::filesystem::remove_all(staging, error);
	OWL_INFO("Settings import: {} file(s) applied.", count)
	return count;
}

}// namespace owl::nest::settings_import
