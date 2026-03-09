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
#include <set>

namespace owl::io::pack {

/**
 * @brief Describes an asset reference found in a scene.
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
 * @brief Scans scene files to discover all referenced assets.
 */
class OWL_API AssetScanner final {
public:
	/**
	 * @brief Scan a single scene file and return all referenced assets.
	 * @param[in] iSceneFile Absolute path to the scene file.
	 * @return All discovered asset references (including the scene itself).
	 */
	[[nodiscard]] static auto scanScene(const std::filesystem::path& iSceneFile) -> std::vector<AssetReference>;

	/**
	 * @brief Scan all scenes reachable from a project's first scene.
	 * @param[in] iProjectDir The project root directory.
	 * @param[in] iFirstScene Relative path to the first scene.
	 * @return Deduplicated list of all assets across all reachable scenes.
	 */
	[[nodiscard]] static auto scanProject(const std::filesystem::path& iProjectDir,
										  const std::string& iFirstScene) -> std::vector<AssetReference>;

private:
	/**
	 * @brief Recursively scan a scene and all scenes it links to via teleports.
	 * @param[in] iSceneFile Scene file to scan.
	 * @param[in,out] ioVisitedScenes Set of already-visited scene paths (avoids cycles).
	 * @param[in,out] ioAssets Accumulated deduplicated assets.
	 */
	static void scanSceneRecursive(const std::filesystem::path& iSceneFile,
								   std::set<std::string>& ioVisitedScenes,
								   std::vector<AssetReference>& ioAssets);

	/**
	 * @brief Collect engine assets required at runtime (shaders, default font).
	 * @param[in,out] ioAssets The asset list to append to.
	 */
	static void collectEngineAssets(std::vector<AssetReference>& ioAssets);

	/**
	 * @brief Resolve a texture serialized string to a disk path.
	 * @param[in] iSerialized The serialized texture string (nam:, pat:, etc.).
	 * @return The resolved reference, or nullopt.
	 */
	[[nodiscard]] static auto resolveTexture(const std::string& iSerialized) -> std::optional<AssetReference>;

	/**
	 * @brief Resolve a font name to a disk path.
	 * @param[in] iFontName The font name.
	 * @return The resolved reference, or nullopt.
	 */
	[[nodiscard]] static auto resolveFont(const std::string& iFontName) -> std::optional<AssetReference>;

	/**
	 * @brief Resolve a teleport level name to a scene file path.
	 * @param[in] iLevelName The level name from the Trigger component.
	 * @return The resolved scene file path, or nullopt.
	 */
	[[nodiscard]] static auto resolveScene(const std::string& iLevelName) -> std::optional<std::filesystem::path>;
};

}// namespace owl::io::pack
