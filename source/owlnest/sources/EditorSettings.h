/**
 * @file EditorSettings.h
 * @author Silmaen
 * @date 16/02/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>
#include <unordered_map>

namespace owl::nest {

/**
 * @brief Structure holding editor-specific settings persisted across sessions.
 */
struct EditorSettings {
	/// Maximum number of recent projects to remember.
	static constexpr size_t maxRecentProjects = 10;

	/// Whether the stats panel is visible.
	bool showStats = true;
	/// Current theme preset name (empty or "Custom" means use theme.yml).
	std::string themePreset = "Dark";
	/// Font size (pixels) used inside the code editor tabs.
	int codeEditorFontSize = 17;
	/// Pixel size of the main UI font.  Applied at the next startup (atlas is built once in
	/// `UiLayer::onAttach`).  Default 18 — the previous default was 20 but 18 leaves more room
	/// for docked panels on dense layouts.
	int uiFontSize = 18;
	/// Custom keybinding overrides (action ID -> shortcut string). Only non-default bindings.
	std::unordered_map<std::string, std::string> keybindingOverrides;
	/// Recently opened project directories (most recent first).
	std::vector<std::string> recentProjects;

	/**
	 * @brief Add a project directory to the recent projects list (most recent first, capped at maxRecentProjects).
	 * @param[in] iProjectDir The project directory to add.
	 */
	void pushRecentProject(const std::filesystem::path& iProjectDir);

	/**
	 * @brief Remove a project directory from the recent projects list.
	 * @param[in] iProjectDir The project directory to remove.
	 */
	void removeRecentProject(const std::filesystem::path& iProjectDir);

	/**
	 * @brief Load settings from a YAML file.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief Save settings to a YAML file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;
};

}// namespace owl::nest
