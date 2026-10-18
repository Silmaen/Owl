/**
 * @file AssetSearchPaths.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/AssetSearchPaths.h"

#include "app/Application.h"

#include <filesystem>
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

}// namespace owl::data::assets
