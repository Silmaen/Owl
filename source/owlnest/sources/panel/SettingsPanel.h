/**
 * @file SettingsPanel.h
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../ActionRegistry.h"
#include "../EditorSettings.h"

namespace owl::nest::panel {

/**
 * @brief Dockable settings window with theme selection and keybinding editor.
 */
class SettingsPanel final {
public:
	SettingsPanel() = default;
	~SettingsPanel() = default;
	SettingsPanel(const SettingsPanel&) = default;
	SettingsPanel(SettingsPanel&&) = default;
	auto operator=(const SettingsPanel&) -> SettingsPanel& = default;
	auto operator=(SettingsPanel&&) -> SettingsPanel& = default;

	/// Show the settings window.
	void open() { m_visible = true; }

	/**
	 * @brief Render the settings window. Call every frame.
	 * @param[in,out] ioSettings Editor settings to read/modify.
	 * @param[in,out] ioRegistry Action registry for keybinding display/editing.
	 */
	void onImGuiRender(EditorSettings& ioSettings, ActionRegistry& ioRegistry);

private:
	/// Whether the window is visible.
	bool m_visible = false;

	/// Whether we are currently capturing a key for rebinding.
	bool m_capturing = false;
	/// The action ID being rebound.
	std::string m_captureActionId;
	/// Conflict warning message (empty = no conflict).
	std::string m_conflictMessage;

	static void renderGeneralSection(EditorSettings& ioSettings);
	static void renderThemeSection(EditorSettings& ioSettings);
	void renderKeybindingsSection(EditorSettings& ioSettings, ActionRegistry& ioRegistry);
	/// Handle key capture logic for a single action row.
	void handleKeyCapture(ActionEntry& ioAction, ActionRegistry& ioRegistry);
	/// Convert an ImGui key enum to a GLFW key code.
	static auto ImGuiKeyToGlfwKey(ImGuiKey iKey) -> int;
};

}// namespace owl::nest::panel
