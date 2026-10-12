/**
 * @file Backstage.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  What the File screen shows and does, filled by the editor each frame.
 */
struct BackstageContext {
	/// Name of the open project; empty without one.
	std::string projectName;
	/// Folder of the open project.
	std::filesystem::path projectDirectory;
	/// Recently opened project folders, most recent first.
	std::vector<std::string> recentProjects;
	/// Folder of the sample project shipped with the editor; empty when it is not found.
	std::filesystem::path sampleProject;
	/// Create a project.
	std::function<void()> newProject;
	/// Pick and open a project.
	std::function<void()> openProject;
	/// Open a project folder.
	std::function<void(const std::filesystem::path&)> openRecent;
	/// Forget a recent project.
	std::function<void(const std::filesystem::path&)> forgetRecent;
	/// Save the open project and its documents.
	std::function<void()> saveProject;
	/// Copy the project to a new folder.
	std::function<void()> saveProjectAs;
	/// Package the project as a game.
	std::function<void()> packGame;
	/// Open the editor settings.
	std::function<void()> editorSettings;
	/// Open the project settings.
	std::function<void()> projectSettings;
	/// Open the engine parameters.
	std::function<void()> engineSettings;
	/// Open the help at a page ("getting_started"), or its index when empty.
	std::function<void(const std::string&)> help;
	/// Close the open project.
	std::function<void()> closeProject;
	/// Quit the editor.
	std::function<void()> exit;
};

/**
 * @brief
 *  Find the sample project shipped with the editor: a `sample_project/owl_project.yml` in a start directory or one
 *  of its first parents (the asset folder of a package or of the source tree).
 * @param[in] iStartDirectories Directories to search from, in priority order.
 * @param[in] iMaxDepth How many parents of each directory are searched.
 * @return The folder of the sample project, or nothing when none is found.
 */
[[nodiscard]] auto findSampleProject(const std::vector<std::filesystem::path>& iStartDirectories,
									 uint32_t iMaxDepth = 4) -> std::optional<std::filesystem::path>;

/**
 * @brief
 *  The File screen: covers the editor with the project actions on the left (new, open, save, package, settings,
 *  help, close, exit) and, on the right, the open project and the recent ones.
 *
 * It replaces the former welcome window: the editor opens it at start-up when no project is open. `Escape` or the
 * back button returns to the editor.
 */
class Backstage final {
public:
	/**
	 * @brief
	 *  Show the screen.
	 */
	void open() { m_open = true; }

	/**
	 * @brief
	 *  Hide the screen.
	 */
	void close() { m_open = false; }

	/**
	 * @brief
	 *  Whether the screen is shown.
	 * @return True when open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_open; }

	/**
	 * @brief
	 *  Draw the screen over the main viewport when open; an action closes it.
	 * @param[in] iContext The project state and the actions.
	 */
	void onImGuiRender(const BackstageContext& iContext);

private:
	/// The screen is shown.
	bool m_open = false;
};

}// namespace owl::nest::panel
