/**
 * @file StatusBar.h
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace owl::gui::widgets {
class ToastQueue;
}// namespace owl::gui::widgets

namespace owl::nest::panel {

/**
 * @brief
 *  Editor state shown by the status bar, gathered by the editor each frame.
 */
struct StatusInfo {
	/**
	 * @brief
	 *  Simulation state of the active scene.
	 */
	enum struct Mode : uint8_t {
		Edit,///< Editing.
		Play,///< Playing.
		Pause,///< Playing, paused.
	};
	/// Simulation state of the active scene.
	Mode mode = Mode::Edit;
	/// Title of the active document; empty without one.
	std::string document;
	/// Number of entities of the active scene; empty when the document is not a scene.
	std::optional<uint32_t> entityCount;
	/// Seconds since the last autosave that wrote something; empty before the first one.
	std::optional<float> secondsSinceAutosave;
	/// Graphics backend name.
	std::string backend;
	/// Name of the graphics device; empty when unknown.
	std::string device;
	/// Previews of the Content Browser ready and to make; empty when every one is done.
	std::optional<std::pair<size_t, size_t>> previews;
	/// Notifications shown by the bell (history, unread count); null for no bell.
	gui::widgets::ToastQueue* notifications = nullptr;
	/// Warnings logged since startup.
	uint64_t warningCount = 0;
	/// Errors logged since startup.
	uint64_t errorCount = 0;
};

/**
 * @brief
 *  One-line bar under the docked panels: simulation mode, active document, autosave, backend and the warnings and
 *  errors not looked at yet.
 *
 * Clicking the warning or error counter acknowledges them and asks the editor to show the Log.
 */
class StatusBar final {
public:
	/**
	 * @brief
	 *  Short human form of a duration in the past.
	 * @param[in] iSeconds Elapsed seconds.
	 * @return "just now", "12 s ago", "3 min ago" or "2 h ago".
	 */
	[[nodiscard]] static auto formatElapsed(float iSeconds) -> std::string;

	/**
	 * @brief
	 *  Label of a simulation mode.
	 * @param[in] iMode The mode.
	 * @return "EDIT", "PLAYING" or "PAUSED".
	 */
	[[nodiscard]] static auto getModeLabel(StatusInfo::Mode iMode) -> std::string;

	/**
	 * @brief
	 *  Warnings logged since the last acknowledgement.
	 * @param[in] iTotal Warnings logged since startup.
	 * @return The unseen warnings.
	 */
	[[nodiscard]] auto getUnseenWarnings(const uint64_t iTotal) const -> uint64_t {
		return iTotal > m_seenWarnings ? iTotal - m_seenWarnings : 0;
	}

	/**
	 * @brief
	 *  Errors logged since the last acknowledgement.
	 * @param[in] iTotal Errors logged since startup.
	 * @return The unseen errors.
	 */
	[[nodiscard]] auto getUnseenErrors(const uint64_t iTotal) const -> uint64_t {
		return iTotal > m_seenErrors ? iTotal - m_seenErrors : 0;
	}

	/**
	 * @brief
	 *  Mark the current warnings and errors as seen.
	 * @param[in] iWarnings Warnings logged since startup.
	 * @param[in] iErrors Errors logged since startup.
	 */
	void acknowledge(const uint64_t iWarnings, const uint64_t iErrors) {
		m_seenWarnings = iWarnings;
		m_seenErrors = iErrors;
	}

	/**
	 * @brief
	 *  Draw the bar at the cursor of the current window, over its whole width.
	 * @param[in] iInfo The state to show.
	 */
	void onImGuiRender(const StatusInfo& iInfo);

	/**
	 * @brief
	 *  Whether the user asked to see the Log since the last call, then reset the request.
	 * @return True once per click on a counter.
	 */
	[[nodiscard]] auto consumeOpenLogRequest() -> bool {
		const bool request = m_openLogRequested;
		m_openLogRequested = false;
		return request;
	}

private:
	/// Warnings already seen.
	uint64_t m_seenWarnings = 0;
	/// Errors already seen.
	uint64_t m_seenErrors = 0;
	/// A counter was clicked.
	bool m_openLogRequested = false;
};

}// namespace owl::nest::panel
