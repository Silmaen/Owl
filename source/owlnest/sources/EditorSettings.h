/**
 * @file EditorSettings.h
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owlgui.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  What the editor restores when a project is opened again: open tabs, active tab and selection.
 */
struct ProjectSession {
	/// Files of the open documents, in tab order, relative to the project when inside it.
	std::vector<std::string> documents;
	/// File of the active document (same form as `documents`); empty when none.
	std::string activeDocument;
	/// UUID of the entity selected in the active scene; 0 when none.
	uint64_t selectedEntity = 0;

	/**
	 * @brief
	 *  Form a file is stored in: relative to the project when inside it, absolute otherwise.
	 * @param[in] iProjectDir The project directory.
	 * @param[in] iFile The absolute file.
	 * @return The stored form, generic separators.
	 */
	[[nodiscard]] static auto toStored(const std::filesystem::path& iProjectDir, const std::filesystem::path& iFile)
			-> std::string;

	/**
	 * @brief
	 *  Absolute file of a stored entry.
	 * @param[in] iProjectDir The project directory.
	 * @param[in] iStored The stored form.
	 * @return The absolute file.
	 */
	[[nodiscard]] static auto resolve(const std::filesystem::path& iProjectDir, const std::string& iStored)
			-> std::filesystem::path;
};

/**
 * @brief
 *  Structure holding editor-specific settings persisted across sessions.
 */
struct EditorSettings {
	/// Maximum number of recent projects to remember.
	static constexpr size_t maxRecentProjects = 10;

	/// Whether the stats panel is visible.
	bool showStats = true;
	/// Theme preset name (`gui::Theme::getPresetNames`); retired names are mapped on load.
	std::string themePreset = "Owl Dark";
	/// Font size (pixels) used inside the code editor tabs.
	int codeEditorFontSize = 17;
	/**
	 * Pixel size of the main UI font.  Applied at the next startup (atlas is built once in
	 * `UiLayer::onAttach`).
	 */
	int uiFontSize = 16;
	/// Scale of every text of the editor, in percent (75 to 150), applied at once.
	int textScalePercent = 100;
	/// Whether the gizmos snap in Edit mode (translation to the grid step, rotation to `snapRotationDegrees`).
	bool snapEnabled = false;
	/// Grid step (world units) used when no tilemap is available or `snapAutoFromTilemap` is off.
	float snapStep = 1.0f;
	/// Multiplier applied to the tilemap's `cellSize` when `snapAutoFromTilemap` is on (e.g. 0.5 = half-cell snap).
	float snapMultiplier = 1.0f;
	/// Angle step (degrees, 0.1 to 180) of the rotation gizmo when it snaps.
	float snapRotationDegrees = 45.0f;
	/// When true, the snap step is taken from the first `Tilemap` component's `cellSize` in the active scene; falls back to `snapStep` when no tilemap is present.
	bool snapAutoFromTilemap = true;
	/// Whether the in-viewport camera markers (icon + forward arrow + FOV cone) are drawn in Edit mode.
	bool showCameraGizmos = true;
	/// Whether the viewport shows, in a corner, the view of the selected camera entity (Edit mode).
	bool showCameraPreview = true;
	/// Whether the frame-time chip (fps, ms, draw calls) is drawn at the bottom-left of the viewports.
	bool showFrameChip = true;
	/// Whether the viewport draws a grid of the snap step in Edit mode.
	bool showGrid = true;
	/// Whether the 3D perspective view draws the grid on the ground (XZ plane, `y = 0`) instead of the XY plane.
	bool gridOnGround = true;
	/// Whether the viewport draws the pathfinding graph of the tilemaps and the test path (isometric Edit mode).
	bool showNavGraph = false;
	/// Whether the trigger type icons are drawn over the triggers in Edit mode.
	bool showTriggerIcons = true;
	/// Whether the gizmo moves along the world axes instead of the axes of the selection.
	bool gizmoWorldSpace = false;
	/// Whether the Play controls of the viewport are folded into a corner handle while the scene runs.
	bool playControlsCollapsed = false;
	/// Speed of the orbit and look-around drags of the editor camera, in percent (10 to 300).
	int cameraRotatePercent = 100;
	/// Speed of the pan drags of the editor camera, in percent (10 to 300).
	int cameraPanPercent = 100;
	/// Speed of the wheel zoom and the dolly of the editor camera, in percent (10 to 300).
	int cameraZoomPercent = 100;
	/// Whether the vertical drag of the orbit and look-around goes the other way.
	bool cameraInvertY = false;
	/// Pixels the cursor travels before a camera drag moves the view (0 to 20).
	int cameraDeadZone = 0;
	/// Seconds between two autosaves of the modified documents (crash recovery); 0 disables the autosave.
	int autosaveIntervalSeconds = 60;
	/// Whether the MCP server listens on 127.0.0.1 (off by default: no socket is opened).
	bool mcpEnabled = false;
	/// Port of the MCP server (1024 to 65535).
	int mcpPort = 47800;
	/// Panel arrangement (`DockLayout::getPresetName`).
	std::string layoutPreset = "Default";
	/// Generation of the saved dock layout; an older one is rebuilt once (`DockLayout::version`).
	int layoutVersion = 0;
	/// Custom keybinding overrides (action ID -> shortcut string). Only non-default bindings.
	std::unordered_map<std::string, std::string> keybindingOverrides;
	/// Lines typed in the console, oldest first (at most `session::Console::maxHistory`).
	std::vector<std::string> consoleHistory;
	/// Recently opened project directories (most recent first).
	std::vector<std::string> recentProjects;
	/// Last session of each recent project, keyed like `recentProjects`.
	std::unordered_map<std::string, ProjectSession> projectSessions;

	/**
	 * @brief
	 *  Remember the session of a project (replaces the previous one).
	 * @param[in] iProjectDir The project directory.
	 * @param[in] iSession The session.
	 */
	void setProjectSession(const std::filesystem::path& iProjectDir, const ProjectSession& iSession);

	/**
	 * @brief
	 *  Get the last session of a project.
	 * @param[in] iProjectDir The project directory.
	 * @return The session, or nothing when none was saved.
	 */
	[[nodiscard]] auto getProjectSession(const std::filesystem::path& iProjectDir) const
			-> std::optional<ProjectSession>;

	/**
	 * @brief
	 *  Add a project directory to the recent projects list (most recent first, capped at maxRecentProjects).
	 * @param[in] iProjectDir The project directory to add.
	 */
	void pushRecentProject(const std::filesystem::path& iProjectDir);

	/**
	 * @brief
	 *  Remove a project directory from the recent projects list.
	 * @param[in] iProjectDir The project directory to remove.
	 */
	void removeRecentProject(const std::filesystem::path& iProjectDir);

	/**
	 * @brief
	 *  Load settings from a YAML file.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief
	 *  Save settings to a YAML file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;
};

}// namespace owl::nest
