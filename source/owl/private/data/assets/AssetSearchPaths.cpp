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

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
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

auto readFileText(const std::filesystem::path& iPath) -> std::optional<std::string> {
	std::error_code error;
	if (!std::filesystem::is_regular_file(iPath, error))
		return std::nullopt;
	std::ifstream file(iPath, std::ios::binary);
	if (!file.is_open())
		return std::nullopt;
	return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

}// namespace

auto readAssetText(const std::filesystem::path& iReference) -> std::optional<std::string> {
	if (iReference.empty())
		return std::nullopt;
	const std::filesystem::path relative{toAssetPath(iReference.generic_string())};
	if (app::Application::instanced()) {
		const auto& app = app::Application::get();
		if (const std::string rel = relative.generic_string(); app.hasOpenPack() && app.packContains(rel)) {
			if (const auto data = app.loadFromPack(rel); data)
				return std::string{data->begin(), data->end()};
		}
	}
	for (const auto& directory: getAssetSearchPaths()) {
		if (auto text = readFileText(directory / relative); text)
			return text;
	}
	return readFileText(relative);
}

}// namespace owl::data::assets
