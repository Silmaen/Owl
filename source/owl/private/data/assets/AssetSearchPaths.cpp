/**
 * @file AssetSearchPaths.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/AssetSearchPaths.h"

#include "data/assets/AssetReference.h"

#include "app/Application.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace owl::data::assets {

auto getAssetSearchPaths() -> std::vector<std::filesystem::path> {
	if (!app::Application::instanced())
		return {std::filesystem::current_path()};
	std::vector<std::filesystem::path> paths;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) paths.push_back(assetsPath);
	return paths;
}

auto isInDirectory(const std::filesystem::path& iPath, const std::filesystem::path& iDirectory) -> bool {
	if (iPath.empty() || iDirectory.empty())
		return false;
	const auto file = std::filesystem::absolute(iPath).lexically_normal();
	const auto directory = std::filesystem::absolute(iDirectory).lexically_normal();
	const auto relative = file.lexically_relative(directory);
	return !relative.empty() && *relative.begin() != "..";
}

namespace {

auto readFileBytes(const std::filesystem::path& iPath) -> std::optional<std::vector<uint8_t>> {
	std::error_code error;
	if (!std::filesystem::is_regular_file(iPath, error))
		return std::nullopt;
	std::ifstream file(iPath, std::ios::binary);
	if (!file.is_open())
		return std::nullopt;
	return std::vector<uint8_t>{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto endsWithEntry(const std::string_view iEntry, const std::string_view iName) -> bool {
	return iEntry.size() > iName.size() && iEntry.ends_with(iName) && iEntry[iEntry.size() - iName.size() - 1] == '/';
}

}// namespace

auto readAssetBytes(const std::filesystem::path& iReference) -> std::optional<std::vector<uint8_t>> {
	if (iReference.empty())
		return std::nullopt;
	const std::filesystem::path relative{toAssetPath(iReference.generic_string())};
	if (app::Application::instanced()) {
		const auto& app = app::Application::get();
		if (const std::string rel = relative.lexically_normal().generic_string();
			app.hasOpenPack() && app.packContains(rel)) {
			if (auto data = app.loadFromPack(rel); data)
				return data;
		}
	}
	for (const auto& directory: getAssetSearchPaths()) {
		if (auto bytes = readFileBytes(directory / relative); bytes)
			return bytes;
	}
	return readFileBytes(relative);
}

auto isPackedAsset(const std::filesystem::path& iPath) -> bool {
	if (iPath.empty() || iPath.is_absolute() || !app::Application::instanced())
		return false;
	const auto& app = app::Application::get();
	return app.hasOpenPack() && app.packContains(iPath.lexically_normal().generic_string());
}

auto findPackedAsset(const std::string& iName, const std::vector<std::string>& iExtensions)
		-> std::optional<std::filesystem::path> {
	if (iName.empty() || !app::Application::instanced() || !app::Application::get().hasOpenPack())
		return std::nullopt;
	const auto& app = app::Application::get();
	const std::filesystem::path name{std::filesystem::path{iName}.lexically_normal()};
	std::vector<std::string> candidates;
	if (name.has_extension())
		candidates.push_back(name.generic_string());
	else
		for (const auto& extension: iExtensions) candidates.push_back(name.generic_string() + extension);
	for (const auto& candidate: candidates) {
		if (app.packContains(candidate))
			return candidate;
	}
	for (const auto& entry: app.getPackReader().listEntries()) {
		for (const auto& candidate: candidates) {
			if (endsWithEntry(entry, candidate))
				return entry;
		}
	}
	return std::nullopt;
}

auto readAssetText(const std::filesystem::path& iReference) -> std::optional<std::string> {
	auto bytes = readAssetBytes(iReference);
	if (!bytes)
		return std::nullopt;
	return std::string{bytes->begin(), bytes->end()};
}

}// namespace owl::data::assets
