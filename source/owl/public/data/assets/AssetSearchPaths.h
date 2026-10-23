/**
 * @file AssetSearchPaths.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief
 *  Asset management.
 */
namespace owl::data::assets {

/**
 * @brief
 *  Directories the asset libraries search, in priority order.
 * The asset directories of the running application, or the working directory when there is none. Defined in the
 * engine library, so the `data` module does not depend on `app`.
 * @return The directories.
 */
[[nodiscard]] OWL_API auto getAssetSearchPaths() -> std::vector<std::filesystem::path>;

/**
 * @brief
 *  Check whether a file lies inside a directory, at any depth (paths compared absolute and lexically normalized).
 * @param[in] iPath The file.
 * @param[in] iDirectory The directory.
 * @return True when the file is the directory or below it; false for an empty path or directory.
 */
[[nodiscard]] OWL_API auto isInDirectory(const std::filesystem::path& iPath, const std::filesystem::path& iDirectory)
		-> bool;

/**
 * @brief
 *  Read the text of an asset: from the open pack first, then from the asset search paths, then the path itself.
 * @param[in] iRelative Path of the asset, relative to an asset directory (or a path on disk).
 * @return The text, or nothing when no source holds the asset.
 */
[[nodiscard]] OWL_API auto readAssetText(const std::filesystem::path& iRelative) -> std::optional<std::string>;

}// namespace owl::data::assets
