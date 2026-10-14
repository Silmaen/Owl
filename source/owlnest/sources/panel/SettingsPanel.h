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

#include <string>

namespace owl::nest::panel {

/**
 * @brief
 *  State of the MCP server shown in the settings window.
 */
struct McpSettingsInfo {
	/// One-line state (`Off`, `Listening`, `Client active`, `Cannot listen on port N`).
	std::string status;
	/// Endpoint URL; empty when off.
	std::string endpoint;
	/// File holding the bearer token.
	std::string tokenFile;
};

/**
 * @brief
 *  Dockable settings window with theme selection and keybinding editor.
 */
class SettingsPanel final {
public:
	SettingsPanel() = default;

	~SettingsPanel() = default;

	SettingsPanel(const SettingsPanel&) = default;

	SettingsPanel(SettingsPanel&&) = default;

	auto operator=(const SettingsPanel&) -> SettingsPanel& = default;

	auto operator=(SettingsPanel&&) -> SettingsPanel& = default;

	/**
	 * @brief
	 *  Show the settings window.
	 */
	void open() { m_visible = true; }

	/**
	 * @brief
	 *  Render the settings window. Call every frame.
	 * @param[in,out] ioSettings Editor settings to read/modify.
	 * @param[in,out] ioRegistry Action registry for keybinding display/editing.
	 */
	void onImGuiRender(EditorSettings& ioSettings, ActionRegistry& ioRegistry);

	/**
	 * @brief
	 *  Show the MCP server tab directly.
	 */
	void openMcp() {
		m_visible = true;
		m_selectMcpTab = true;
	}

	/**
	 * @brief
	 *  Set the state of the MCP server shown by its tab.
	 * @param[in] iInfo The state.
	 */
	void setMcpInfo(const McpSettingsInfo& iInfo) { m_mcpInfo = iInfo; }

	/**
	 * @brief
	 *  Check whether the user asked for a new MCP token since the last call, and clear the request.
	 * @return True when asked.
	 */
	[[nodiscard]] auto consumeRegenerateTokenRequest() -> bool {
		const bool request = m_regenerateTokenRequested;
		m_regenerateTokenRequested = false;
		return request;
	}

	/**
	 * @brief
	 *  Render the MCP server settings (also used by the tests).
	 * @param[in,out] ioSettings Editor settings to read/modify.
	 */
	void renderMcpSection(EditorSettings& ioSettings);

private:
	/// Whether the window is visible.
	bool m_visible = false;
	/// Whether we are currently capturing a key for rebinding.
	bool m_capturing = false;
	/// The action ID being rebound.
	std::string m_captureActionId;
	/// Conflict warning message (empty = no conflict).
	std::string m_conflictMessage;
	/// State of the MCP server.
	McpSettingsInfo m_mcpInfo;
	/// The user asked for a new MCP token.
	bool m_regenerateTokenRequested = false;
	/// Select the MCP tab on the next frame.
	bool m_selectMcpTab = false;

	/**
	 * @brief
	 *  General tab: stats panel, font sizes, autosave.
	 * @param[in,out] ioSettings Editor settings being modified.
	 */
	static void renderGeneralSection(EditorSettings& ioSettings);

	/**
	 * @brief
	 *  Gizmo snap tab.
	 * @param[in,out] ioSettings Editor settings being modified.
	 */
	static void renderSnapSection(EditorSettings& ioSettings);

	/**
	 * @brief
	 *  Camera tab: speeds, vertical inversion and dead zone of the viewport navigation.
	 * @param[in,out] ioSettings Editor settings being modified.
	 */
	static void renderCameraSection(EditorSettings& ioSettings);

	/**
	 * @brief
	 *  Appearance tab: the theme.
	 * @param[in,out] ioSettings Editor settings being modified.
	 */
	static void renderThemeSection(EditorSettings& ioSettings);

	/**
	 * @brief
	 *  Render keybindings section.
	 * @param[in,out] ioSettings Editor settings being modified.
	 * @param[in,out] ioRegistry Action registry whose bindings are edited.
	 */
	void renderKeybindingsSection(EditorSettings& ioSettings, ActionRegistry& ioRegistry);

	/**
	 * @brief
	 *  Handle key capture logic for a single action row.
	 */
	void handleKeyCapture(ActionEntry& ioAction, ActionRegistry& ioRegistry);

	/**
	 * @brief
	 *  Convert an ImGui key enum to a GLFW key code.
	 * @return The int.
	 */
	static auto ImGuiKeyToGlfwKey(ImGuiKey iKey) -> int;
};

}// namespace owl::nest::panel
