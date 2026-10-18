/**
 * @file TilesetUsage.h
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::viewer {

/**
 * @brief
 *  How a tileset cuts an image into tiles.
 */
struct TilesetGrid {
	/// File name of the tileset.
	std::string tileset;
	/// Width of a tile in image pixels.
	uint32_t tileWidth = 1;
	/// Height of a tile in image pixels.
	uint32_t tileHeight = 1;
	/// Number of columns.
	uint32_t columns = 1;
	/// Number of rows.
	uint32_t rows = 1;
	/// True when the tileset samples its atlas with the nearest filter (pixel art).
	bool nearest = false;
};

/**
 * @brief
 *  Check whether the texture reference saved in a tileset (`nam:` asset-relative name or `pat:` path) is an image.
 * @param[in] iReference The reference.
 * @param[in] iImage Path of the image.
 * @param[in] iAssetDirectories The asset folders a `nam:` reference is relative to.
 * @return True when both name the same file.
 */
[[nodiscard]] auto isTextureReference(std::string_view iReference, const std::filesystem::path& iImage,
									  std::span<const std::filesystem::path> iAssetDirectories) -> bool;

/**
 * @brief
 *  Read a tileset document and tell whether it uses an image as its atlas.
 * @param[in] iYaml The `.owltileset` text.
 * @param[in] iTilesetName File name of the tileset, copied in the result.
 * @param[in] iImage Path of the image.
 * @param[in] iAssetDirectories The asset folders a `nam:` reference is relative to.
 * @return The grid of the tileset, nothing when it uses another image or is not a tileset.
 */
[[nodiscard]] auto parseTilesetGrid(std::string_view iYaml, const std::string& iTilesetName,
									const std::filesystem::path& iImage,
									std::span<const std::filesystem::path> iAssetDirectories)
		-> std::optional<TilesetGrid>;

/**
 * @brief
 *  Find the tilesets of a folder tree that use an image as their atlas.
 * @param[in] iRoots The folders searched recursively.
 * @param[in] iImage Path of the image.
 * @param[in] iAssetDirectories The asset folders a `nam:` reference is relative to.
 * @return The grids, in file order.
 */
[[nodiscard]] auto findTilesetGrids(std::span<const std::filesystem::path> iRoots, const std::filesystem::path& iImage,
									std::span<const std::filesystem::path> iAssetDirectories)
		-> std::vector<TilesetGrid>;

}// namespace owl::nest::viewer
