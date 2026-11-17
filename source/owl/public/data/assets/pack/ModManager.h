/**
 * @file ModManager.h
 * @author Silmaen
 * @date 16/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "ExtensionManifest.h"

#include "core/Core.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::data::assets::pack {

class ExtensionRegistry;

/**
 * @brief
 *  A mod the player can enable, disable and move: one pack of the mods folder of a game.
 */
struct OWL_API ModEntry {
	/// File or folder of the pack.
	std::filesystem::path source;
	/// Its manifest.
	ExtensionManifest manifest;
	/// Chosen by the player (all are, until the player disables one).
	bool enabled{true};
	/// Mounted by the last `apply`: an enabled mod left out by its dependencies is not.
	bool mounted{false};
	/// Why the last `apply` left it out; empty when mounted or disabled.
	std::string issue;
};

/**
 * @brief
 *  The choices of the player over the mods of a game: which ones are enabled and in what order they load.
 *
 * The mods are the packs of the mods folder; the choices are saved in a YAML file of the user folder of the game
 * (`mods.yml`), so they survive an update of the game and of the mods. A mod placed lower in the list loads later and
 * wins the assets it shares with the ones above it; its dependencies still load first.
 */
class OWL_API ModManager final {
public:
	/**
	 * @brief
	 *  Set the mods folder and the file of the choices, then read both.
	 * @param[in] iModsFolder Folder of the `.owlmod` files and pack folders.
	 * @param[in] iSettingsFile YAML file of the choices; none when empty (nothing saved).
	 */
	void configure(const std::filesystem::path& iModsFolder, const std::filesystem::path& iSettingsFile);

	/**
	 * @brief
	 *  Whether a mods folder is set.
	 * @return True after `configure`.
	 */
	[[nodiscard]] auto isConfigured() const -> bool { return !m_folder.empty(); }

	/**
	 * @brief
	 *  Read the packs of the mods folder again: the known ones keep their place and their choice, a new one is enabled
	 *  and placed after them by its `loadOrder`, a pack no longer there is dropped.
	 */
	void rescan();

	/**
	 * @brief
	 *  The mods, in the order the player chose.
	 * @return The mods.
	 */
	[[nodiscard]] auto getMods() const -> const std::vector<ModEntry>& { return m_mods; }

	/**
	 * @brief
	 *  Find a mod by id.
	 * @param[in] iId Id of its pack.
	 * @return The mod, or null.
	 */
	[[nodiscard]] auto find(const std::string& iId) const -> const ModEntry*;

	/**
	 * @brief
	 *  Enable or disable a mod; takes effect at the next `apply`.
	 * @param[in] iId Id of its pack.
	 * @param[in] iEnabled The choice.
	 * @return False when no mod has that id.
	 */
	auto setEnabled(const std::string& iId, bool iEnabled) -> bool;

	/**
	 * @brief
	 *  Move a mod in the list; takes effect at the next `apply`.
	 * @param[in] iId Id of its pack.
	 * @param[in] iOffset Places to move: negative towards the top (loads earlier), positive towards the bottom.
	 * @return False when no mod has that id or it does not move.
	 */
	auto move(const std::string& iId, int32_t iOffset) -> bool;

	/**
	 * @brief
	 *  Whether a choice changed since the last `apply`.
	 * @return True when the mounted packs differ from the choices.
	 */
	[[nodiscard]] auto isDirty() const -> bool { return m_dirty; }

	/**
	 * @brief
	 *  Sources of the enabled mods, in the order of the list.
	 * @return The sources.
	 */
	[[nodiscard]] auto getEnabledSources() const -> std::vector<std::filesystem::path>;

	/**
	 * @brief
	 *  Rank of every mod by id, its place in the list, to give `ExtensionRegistry::mount`.
	 * @return The ranks.
	 */
	[[nodiscard]] auto getLoadOrder() const -> std::unordered_map<std::string, int32_t>;

	/**
	 * @brief
	 *  Record what a registry mounted: the `mounted` flag and the issue of each mod, then the dirty flag cleared.
	 * @param[in] iRegistry The registry the enabled sources were mounted in.
	 */
	void update(const ExtensionRegistry& iRegistry);

	/**
	 * @brief
	 *  Save the choices to the file given to `configure`.
	 * @return False when there is no file or it cannot be written.
	 */
	[[nodiscard]] auto save() const -> bool;

	/**
	 * @brief
	 *  Forget the folder, the file and the mods.
	 */
	void reset();

private:
	/**
	 * @brief
	 *  Read the choices of the settings file into the order and the disabled ids.
	 */
	void load();
	/// Mods folder.
	std::filesystem::path m_folder;
	/// File of the choices.
	std::filesystem::path m_settingsFile;
	/// The mods, in the order of the player.
	std::vector<ModEntry> m_mods;
	/// Order saved by the player, by id (read from the file, before the scan).
	std::vector<std::string> m_savedOrder;
	/// Ids the player disabled.
	std::vector<std::string> m_disabled;
	/// A choice changed since the last `update`.
	bool m_dirty{false};
};

}// namespace owl::data::assets::pack
