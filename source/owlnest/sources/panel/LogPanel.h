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
 * @brief Dockable panel displaying engine logs with filtering.
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
	 * @brief Render the log panel.
	 */
	void onImGuiRender();

private:
	/// Level filters.
	bool m_showTrace = true;
	bool m_showDebug = true;
	bool m_showInfo = true;
	bool m_showWarning = true;
	bool m_showError = true;
	bool m_showCritical = true;
	/// Logger filters.
	bool m_showCore = true;
	bool m_showApp = true;
	/// Text search buffer.
	std::array<char, 256> m_searchBuffer{};
	/// Auto-scroll to bottom.
	bool m_autoScroll = true;
};

}// namespace owl::nest::panel
