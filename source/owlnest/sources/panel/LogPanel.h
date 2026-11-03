/**
 * @file LogPanel.h
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/Log.h>
#include <debug/LogSink.h>
#include <owlgui.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  Log level an editor setting names.
 * @param[in] iSetting Level name (`trace` to `off`), empty for the program default (`Log::getDefaultLevel`).
 * @return The level.
 */
[[nodiscard]] auto getLogLevelSetting(const std::string& iSetting) -> core::Log::Level;

/**
 * @brief
 *  Draw the log level combo: *Default* then every level; a choice is saved in the setting and applied at once.
 * @param[in] iId Unique ImGui identifier.
 * @param[in,out] ioSetting The level setting (empty for the default).
 * @return True when the level changed.
 */
auto logLevelCombo(const char* iId, std::string& ioSetting) -> bool;

/**
 * @brief
 *  Dockable panel displaying engine logs with filtering.
 */
class LogPanel final {
public:
	LogPanel() = default;

	~LogPanel() = default;

	LogPanel(const LogPanel&) = default;

	LogPanel(LogPanel&&) = default;

	auto operator=(const LogPanel&) -> LogPanel& = default;

	auto operator=(LogPanel&&) -> LogPanel& = default;

	/**
	 * @brief
	 *  Render the log panel.
	 * @param[in,out] ioLevelSetting The log level editor setting, changed by the level combo of the toolbar.
	 */
	void onImGuiRender(std::string& ioLevelSetting);

private:
	/// Show trace-level entries.
	bool m_showTrace = true;
	/// Show debug-level entries.
	bool m_showDebug = true;
	/// Show info-level entries.
	bool m_showInfo = true;
	/// Show warning-level entries.
	bool m_showWarning = true;
	/// Show error-level entries.
	bool m_showError = true;
	/// Show critical-level entries.
	bool m_showCritical = true;
	/// Show entries from the engine (`OWL_CORE_*`) channel.
	bool m_showCore = true;
	/// Show entries from the application (`OWL_*`) channel.
	bool m_showApp = true;
	/// Text search buffer.
	std::array<char, 256> m_searchBuffer{};
	/// Keep the view on the latest message; scrolling up turns it off, scrolling back to the bottom turns it on.
	bool m_autoScroll = true;
	/// Show the messages as plain text that can be selected and copied.
	bool m_textMode = false;
	/// Line the context menu was opened on, in the shown lines.
	size_t m_menuLine = 0;
	/// Text of the plain text view.
	std::string m_selectableText;

	/**
	 * @brief
	 *  Whether a message passes the level, logger and text filters.
	 * @param[in] iEntry The message.
	 * @return True when it is shown.
	 */
	[[nodiscard]] auto isShown(const debug::LogEntry& iEntry) const -> bool;

	/**
	 * @brief
	 *  Draw the shown messages in colour, with the follow scroll and the copy menu.
	 * @param[in] iShown The messages.
	 */
	void renderColoredLog(const std::vector<const debug::LogEntry*>& iShown);

	/**
	 * @brief
	 *  Draw the shown messages as read-only plain text, to select and copy.
	 * @param[in] iShown The messages.
	 */
	void renderSelectableText(const std::vector<const debug::LogEntry*>& iShown);
};

}// namespace owl::nest::panel
