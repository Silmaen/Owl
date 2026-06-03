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

namespace YAML {

class Node;
}// namespace YAML

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

private:
	/**
	 * @brief
	 *  Recursively scan a scene and all scenes it links to via teleports.
	 * @param[in] iSceneFile Scene file to scan.
	 * @param[in,out] ioVisitedScenes Set of already-visited scene paths (avoids cycles).
	 * @param[in,out] ioAssets Accumulated deduplicated assets.
	 * @param[in,out] ioWarnings Optional pointer to collect unresolved-reference warnings.
	 */
	static void scanSceneRecursive(const std::filesystem::path& iSceneFile, std::set<std::string>& ioVisitedScenes,
								   std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings);

	/**
	 * @brief
	 * Scan a single entity node for referenced assets.
	 * @param[in] iEntity The YAML node representing the entity.
	 * @param[in] iSceneName The name of the scene containing this entity (for warning messages).
	 * @param[in,out] ioVisitedScenes Set of already-visited scene paths (for nested scene references).
	 * @param[in,out] ioAssets Accumulated deduplicated assets.
	 * @param[in,out] ioWarnings Optional pointer to collect unresolved-reference warnings.
	 */
	static void scanEntity(const YAML::Node& iEntity, const std::string& iSceneName,
						   std::set<std::string>& ioVisitedScenes, std::vector<AssetReference>& ioAssets,
						   std::vector<std::string>* ioWarnings);

	/**
	 * @brief
	 *  Resolve and add a tilemap asset and the chain it references: the
	 *  `.owltilemap` file, the `.owltileset` it points at, and that tileset's
	 *  atlas texture.
	 * @param[in] iTilemapPath The `tilemapPath` from a `Tilemap` component
	 *  (relative to an asset directory).
	 * @param[in] iSceneName The scene containing the component (for warnings).
	 * @param[in,out] ioAssets Accumulated deduplicated assets.
	 * @param[in,out] ioWarnings Optional pointer to collect unresolved-reference warnings.
	 */
	static void scanTilemap(const std::string& iTilemapPath, const std::string& iSceneName,
							std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings);

	/**
	 * @brief
	 *  Resolve and add a `.owltileset` asset and its atlas texture. Shared by
	 *  the tilemap chain and the raycast door / pushwall components, which
	 *  reference a tileset directly.
	 * @param[in] iTilesetPath The `.owltileset` path (relative to an asset directory).
	 * @param[in] iSceneName The scene containing the reference (for warnings).
	 * @param[in,out] ioAssets Accumulated deduplicated assets.
	 * @param[in,out] ioWarnings Optional pointer to collect unresolved-reference warnings.
	 */
	static void scanTileset(const std::string& iTilesetPath, const std::string& iSceneName,
							std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings);

	/**
	 * @brief
	 *  Scan a Lua script for scene.load_scene() calls and add referenced scenes.
	 * @param[in] iScriptPath Absolute path to the Lua script file.
	 * @param[in,out] ioVisitedScenes Set of already-visited scene paths.
	 * @param[in,out] ioAssets Accumulated assets.
	 * @param[in,out] ioWarnings Optional pointer to collect unresolved-reference warnings.
	 */
	static void scanLuaScriptForScenes(const std::filesystem::path& iScriptPath, std::set<std::string>& ioVisitedScenes,
									   std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings);

	/**
	 * @brief
	 *  Scan a Lua script for sound.play() calls and add referenced sound assets.
	 * @param[in] iScriptPath Absolute path to the Lua script file.
	 * @param[in,out] ioAssets Accumulated assets.
	 * @param[in,out] ioWarnings Optional pointer to collect unresolved-reference warnings.
	 */
	static void scanLuaScriptForSounds(const std::filesystem::path& iScriptPath, std::vector<AssetReference>& ioAssets,
									   std::vector<std::string>* ioWarnings);

	/**
	 * @brief
	 *  Collect engine assets required at runtime (shaders, default font).
	 * @param[in,out] ioAssets The asset list to append to.
	 */
	static void collectEngineAssets(std::vector<AssetReference>& ioAssets);

	/**
	 * @brief
	 *  Resolve a texture serialized string to a disk path.
	 * @param[in] iSerialized The serialized texture string (nam:, pat:, etc.).
	 * @return The resolved reference, or nullopt.
	 */
	[[nodiscard]] static auto resolveTexture(const std::string& iSerialized) -> std::optional<AssetReference>;

	/**
	 * @brief
	 *  Resolve a font name to a disk path.
	 * @param[in] iFontName The font name.
	 * @return The resolved reference, or nullopt.
	 */
	[[nodiscard]] static auto resolveFont(const std::string& iFontName) -> std::optional<AssetReference>;

	/**
	 * @brief
	 *  Resolve a sound asset name to a disk path.
	 * @param[in] iSoundAsset The sound asset relative path.
	 * @return The resolved reference, or nullopt.
	 */
	[[nodiscard]] static auto resolveSound(const std::string& iSoundAsset) -> std::optional<AssetReference>;

	/**
	 * @brief
	 *  Resolve a script path to a disk path.
	 * @param[in] iScriptPath The script relative path from LuaScript component.
	 * @return The resolved reference, or nullopt.
	 */
	[[nodiscard]] static auto resolveScript(const std::string& iScriptPath) -> std::optional<AssetReference>;

	/**
	 * @brief
	 *  Resolve a teleport level name to a scene file path.
	 * @param[in] iLevelName The level name from the Trigger component.
	 * @return The resolved scene file path, or nullopt.
	 */
	[[nodiscard]] static auto resolveScene(const std::string& iLevelName) -> std::optional<std::filesystem::path>;
};

}// namespace owl::data::assets::pack
