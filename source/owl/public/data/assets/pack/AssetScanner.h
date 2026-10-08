/**
 * @file AssetScanner.h
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "PackFormat.h"

#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief
 *  Packing namespace.
 */
namespace owl::data::assets::pack {
/**
 * @brief
 *  Describes an asset reference that is found in a scene.
 */
struct AssetReference {
	/// Relative path for the pack entry.
	std::string packPath;
	/// Absolute path on disk.
	std::filesystem::path diskPath;
	/// The asset type.
	AssetType assetType = AssetType::Other;
};

/**
 * @brief
 *  Scans scene files to discover all referenced assets.
 */
class OWL_API AssetScanner final {
public:
	/**
	 * @brief
	 *  Scan a single scene file and return all referenced assets.
	 * @param[in] iSceneFile Absolute path to the scene file.
	 * @param[out] oWarnings Optional output filled with messages for unresolvable references.
	 * @return All discovered asset references (including the scene itself).
	 */
	[[nodiscard]] static auto scanScene(const std::filesystem::path& iSceneFile,
										std::vector<std::string>* oWarnings = nullptr) -> std::vector<AssetReference>;

	/**
	 * @brief
	 *  Scan all scenes reachable from a project's first scene.
	 * @param[in] iProjectDir The project root directory.
	 * @param[in] iFirstScene Relative path to the first scene.
	 * @param[out] oWarnings Optional output filled with messages for unresolvable references.
	 * @return Deduplicated list of all assets across all reachable scenes.
	 */
	[[nodiscard]] static auto scanProject(const std::filesystem::path& iProjectDir, const std::string& iFirstScene,
										  std::vector<std::string>* oWarnings = nullptr) -> std::vector<AssetReference>;
};

}// namespace owl::data::assets::pack
