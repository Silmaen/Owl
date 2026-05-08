/**
 * @file SaveManager.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"
#include "physic/PhysicCommand.h"

#include <filesystem>

namespace owl::scene {
/**
 * @brief
 *  Information about a save file.
 */
struct OWL_API SaveInfo {
	/// Save slot number.
	uint32_t slot = 0;
	/// Timestamp of the save.
	std::string timestamp;
	/// Scene path at the time of saving.
	std::string scenePath;
};

/**
 * @brief
 *  Manages game save/load to the user's save directory.
 *
 * Save files are stored as YAML in platform-specific user directories.
 * Each slot is a separate file `save_<N>.owl_save`.
 */
class OWL_API SaveManager final {
public:
	SaveManager() = delete;

	/**
	 * @brief
	 *  Set the game name (used for the save directory).
	 * @param[in] iGameName The game name.
	 */
	static void setGameName(const std::string& iGameName);

	/**
	 * @brief
	 *  Get the platform-specific save directory.
	 * @return The save directory path (created if it doesn't exist).
	 */
	[[nodiscard]] static auto getSaveDirectory() -> std::filesystem::path;

	/**
	 * @brief
	 *  Save the current scene and game state to a slot.
	 * @param[in] iSlot Save slot number.
	 * @param[in] iScene The scene to save.
	 * @param[in] iScenePath The current scene file path (for restore).
	 * @return True on success.
	 */
	[[nodiscard]] static auto save(uint32_t iSlot, const shared<Scene>& iScene, const std::string& iScenePath) -> bool;

	/**
	 * @brief
	 *  Result of a load operation.
	 */
	struct LoadResult {
		/// Whether the load succeeded.
		bool success = false;
		/// Physics snapshots to apply after onStartRuntime (keyed by entity UUID).
		std::unordered_map<uint64_t, physic::PhysicCommand::PhysicsSnapshot> physicsSnapshots;
	};

	/**
	 * @brief
	 *  Load a save file into a scene.
	 * @param[in] iSlot Save slot number.
	 * @param[in] iScene The scene to load into (will be cleared).
	 * @return Load result with success flag and physics snapshots.
	 */
	[[nodiscard]] static auto load(uint32_t iSlot, const shared<Scene>& iScene) -> LoadResult;

	/**
	 * @brief
	 *  List all available saves.
	 * @return List of save info (slot, timestamp, scene path).
	 */
	[[nodiscard]] static auto listSaves() -> std::vector<SaveInfo>;

	/**
	 * @brief
	 *  Check if a save exists for a given slot.
	 * @param[in] iSlot Save slot number.
	 * @return True if the save file exists.
	 */
	[[nodiscard]] static auto hasSave(uint32_t iSlot) -> bool;

	/**
	 * @brief
	 *  Delete a save file.
	 * @param[in] iSlot Save slot number.
	 */
	static void deleteSave(uint32_t iSlot);

	/**
	 * @brief
	 *  Get the scene path stored in a save file.
	 * @param[in] iSlot Save slot number.
	 * @return The scene path, or empty string if not found.
	 */
	[[nodiscard]] static auto getScenePath(uint32_t iSlot) -> std::string;

private:
	/// The game name.
	static std::string s_gameName;

	/**
	 * @brief
	 *  Get the file path for a slot.
	 * @return The slot path.
	 */
	[[nodiscard]] static auto getSlotPath(uint32_t iSlot) -> std::filesystem::path;
};

}// namespace owl::scene
