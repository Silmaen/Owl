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

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

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
	 *  Check whether the window is shown.
	 * @return True while it is open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_visible; }

	/**
	 * @brief
	 *  Start editing: copy the settings into the draft the tabs edit and note the shortcuts, so Cancel can put them
	 *  back. Called by the window when it opens.
	 * @param[in] iSettings The settings in use.
	 * @param[in] iRegistry The actions and their shortcuts in use.
	 */
	void beginEdit(const EditorSettings& iSettings, const ActionRegistry& iRegistry);

	/**
	 * @brief
	 *  The settings the tabs edit until Apply or OK.
	 * @return The draft, empty while the window is closed.
	 */
	[[nodiscard]] auto getDraft() -> std::optional<EditorSettings>& { return m_draft; }

	/**
	 * @brief
	 *  Check whether the draft differs from the settings in use, on the fields the window edits.
	 * @param[in] iSettings The settings in use.
	 * @return True when Apply would change something.
	 */
	[[nodiscard]] auto hasPendingChanges(const EditorSettings& iSettings) const -> bool;

	/**
	 * @brief
	 *  Apply: copy the fields the window edits from the draft, then apply the theme, the log level and the frame
	 *  trace period. The rest of the settings (session, layout, recent projects) is left as it is.
	 * @param[in,out] ioSettings The settings in use.
	 * @param[in] iRegistry The shortcuts, kept as the ones Cancel goes back to.
	 */
	void apply(EditorSettings& ioSettings, const ActionRegistry& iRegistry);

	/**
	 * @brief
	 *  Cancel: drop the draft, put back the shortcuts of the last Apply and close the window.
	 * @param[in,out] ioRegistry The actions whose shortcuts go back.
	 */
	void cancel(ActionRegistry& ioRegistry);

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
	 *  Render the Log tab: the lowest level recorded and the period of the per-frame traces (`config.yml`).
	 * @param[in,out] ioSettings The editor settings.
	 */
	void renderLogSection(EditorSettings& ioSettings);

	/**
	 * @brief
	 *  OK, Apply and Cancel under the tabs.
	 * @param[in,out] ioSettings The settings in use.
	 * @param[in,out] ioRegistry The actions and their shortcuts.
	 */
	void renderFooter(EditorSettings& ioSettings, ActionRegistry& ioRegistry);

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

	/// The settings the tabs edit, from the opening of the window to OK or Cancel.
	std::optional<EditorSettings> m_draft;
	/// Shortcut overrides at the opening of the window or the last Apply, put back by Cancel.
	std::unordered_map<std::string, std::string> m_shortcutsAtOpen;
	/// Frames between two lines of a per-frame trace, edited in the Log tab.
	uint64_t m_draftFrameLogFrequency = 0;
};

}// namespace owl::nest::panel
