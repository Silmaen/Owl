/**
 * @file LogPanel.h
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include <array>

namespace owl::nest::panel {
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
	 */
	void onImGuiRender();

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
	/// Auto-scroll to bottom.
	bool m_autoScroll = true;
};

}// namespace owl::nest::panel
