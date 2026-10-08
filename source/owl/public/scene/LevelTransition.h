/**
 * @file LevelTransition.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"
#include "SceneSerializer.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace owl::scene {

/**
 * @brief
 *  The level changes a game asks for (teleport to another level, loading a saved game), shared by the editor Play
 *  mode and the runner so both behave the same.
 *
 * Every step leaves the running level untouched on failure: the caller stops the old level only once the new one
 * is loaded.
 */
class OWL_API LevelTransition final {
public:
	/**
	 * @brief
	 *  The raw bytes of a level and where they came from.
	 */
	struct LevelSource {
		/// File content.
		std::vector<uint8_t> bytes;
		/// Pack entry or file path read, named in the messages.
		std::string sourceName;
	};

	/**
	 * @brief
	 *  Add the `.owl` extension to a level name that has none.
	 * @param[in] iLevelName Level name as written in the trigger or the script.
	 * @return The file name of the level.
	 */
	[[nodiscard]] static auto resolveLevelName(const std::string& iLevelName) -> std::string;

	/**
	 * @brief
	 *  Read a level: the open pack first (`<name>` then `scenes/<name>`), then each root folder (same two names).
	 *
	 * Safe on a worker thread when the roots are copied beforehand (see getSearchRoots).
	 * @param[in] iLevelName Level name, with or without its extension.
	 * @param[in] iRoots Folders searched, in order.
	 * @return The level bytes, or nothing when no pack entry nor file has this name.
	 */
	[[nodiscard]] static auto readLevel(const std::string& iLevelName, const std::vector<std::filesystem::path>& iRoots)
			-> std::optional<LevelSource>;

	/**
	 * @brief
	 *  The asset folders of the application, in search order.
	 * @return The folders.
	 */
	[[nodiscard]] static auto getSearchRoots() -> std::vector<std::filesystem::path>;

	/**
	 * @brief
	 *  Build the next level from a parsed file, carrying over the game state of the current one.
	 * @param[in] iParsed The parsed level.
	 * @param[in] iCurrent The level being left (only read).
	 * @return The new level, not started; nullptr (error logged with its fix) when the file does not load.
	 */
	[[nodiscard]] static auto loadLevel(const ParsedScene& iParsed, const Scene& iCurrent) -> shared<Scene>;

	/**
	 * @brief
	 *  Place the primary player on the arrival entity, its velocity turned by the arrival rotation.
	 *
	 * Call it once the new level runs (its physics bodies exist). Nothing happens without a player or an entity
	 * tagged `iTargetName`.
	 * @param[in,out] ioScene The level reached.
	 * @param[in] iTargetName Tag of the arrival entity.
	 * @param[in] iVelocity Player velocity in the arrival frame.
	 */
	static void placeArrival(Scene& ioScene, const std::string& iTargetName, const math::vec2f& iVelocity);

	/**
	 * @brief
	 *  Load a saved game and start it; the current level stops only when the save loaded.
	 * @param[in] iSlot Save slot.
	 * @param[in,out] ioCurrent The running level, stopped on success.
	 * @param[in] iViewportSize Viewport size given to the loaded level.
	 * @return The running loaded level, or nullptr (current level untouched) when the save does not load.
	 */
	[[nodiscard]] static auto loadSavedGame(uint32_t iSlot, Scene& ioCurrent, const math::vec2ui& iViewportSize)
			-> shared<Scene>;
};

}// namespace owl::scene
