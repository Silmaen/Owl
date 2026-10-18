/**
 * @file TilesetUsage.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilesetUsage.h"

#include "../../AssetKind.h"

#include <core/Log.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::viewer {

namespace {
constexpr std::string_view g_namePrefix = "nam:";
constexpr std::string_view g_pathPrefix = "pat:";

auto sameFile(const std::filesystem::path& iLhs, const std::filesystem::path& iRhs) -> bool {
	return iLhs.lexically_normal() == iRhs.lexically_normal();
}
}// namespace

auto isTextureReference(const std::string_view iReference, const std::filesystem::path& iImage,
						const std::span<const std::filesystem::path> iAssetDirectories) -> bool {
	if (iReference.starts_with(g_pathPrefix))
		return sameFile(std::filesystem::path{iReference.substr(g_pathPrefix.size())}, iImage);
	if (!iReference.starts_with(g_namePrefix))
		return false;
	const std::filesystem::path name{iReference.substr(g_namePrefix.size())};
	return std::ranges::any_of(iAssetDirectories, [&](const std::filesystem::path& iDirectory) -> bool {
		return sameFile(iDirectory / name, iImage);
	});
}

auto parseTilesetGrid(const std::string_view iYaml, const std::string& iTilesetName,
					  const std::filesystem::path& iImage,
					  const std::span<const std::filesystem::path> iAssetDirectories) -> std::optional<TilesetGrid> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& error) {
		OWL_CORE_WARN("TilesetUsage: '{}' is not readable: {}.", iTilesetName, error.what())
		return std::nullopt;
	}
	if (!root.IsMap() || !root["Tileset"] || !root["texture"])
		return std::nullopt;
	try {
		if (!isTextureReference(root["texture"].as<std::string>(), iImage, iAssetDirectories))
			return std::nullopt;
		TilesetGrid grid;
		grid.tileset = iTilesetName;
		grid.tileWidth = std::max(1u, root["tileWidth"].as<uint32_t>(32));
		grid.tileHeight = std::max(1u, root["tileHeight"].as<uint32_t>(32));
		grid.columns = std::max(1u, root["columns"].as<uint32_t>(1));
		grid.rows = std::max(1u, root["rows"].as<uint32_t>(1));
		grid.nearest = root["filterMode"].as<std::string>("Linear") == "Nearest";
		return grid;
	} catch (const YAML::Exception& error) {
		OWL_CORE_WARN("TilesetUsage: '{}' has a bad value: {}.", iTilesetName, error.what())
		return std::nullopt;
	}
}

auto findTilesetGrids(const std::span<const std::filesystem::path> iRoots, const std::filesystem::path& iImage,
					  const std::span<const std::filesystem::path> iAssetDirectories) -> std::vector<TilesetGrid> {
	std::vector<TilesetGrid> grids;
	for (const auto& root: iRoots) {
		for (const auto& relative: listAssets(root, ".owltileset")) {
			const std::ifstream file{root / relative, std::ios::binary};
			if (!file.is_open())
				continue;
			std::ostringstream text;
			text << file.rdbuf();
			if (auto grid = parseTilesetGrid(text.str(), std::filesystem::path{relative}.filename().string(), iImage,
											 iAssetDirectories);
				grid.has_value())
				grids.push_back(std::move(*grid));
		}
	}
	return grids;
}

}// namespace owl::nest::viewer
