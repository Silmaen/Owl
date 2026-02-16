/**
 * @file Parameters.h
 * @author Silmaen
 * @date 16/02/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest::panel {
/**
 * @brief Class Parameters
 */
class Parameters final {
public:
	/**
	 * @brief Default copy constructor
	 */
	Parameters(const Parameters&) = default;
	/**
	 * @brief Default move constructor
	 */
	Parameters(Parameters&&) = default;
	/**
	 * @brief Default copy assignation
	 * @return this
	 */
	auto operator=(const Parameters&) -> Parameters& = default;
	/**
	 * @brief Default move assignation
	 * @return this
	 */
	auto operator=(Parameters&&) -> Parameters& = default;
	/**
	 * @brief Default constructor.
	 */
	Parameters() = default;
	/**
	 * @brief Destructor.
	 */
	~Parameters() = default;

	/**
	 * @brief Open the modal dialog, taking a snapshot of current parameters.
	 */
	void open();

	/**
	 * @brief Action on Gui Render
	 */
	void onImGuiRender();

private:
	/**
	 * @brief Apply the local parameters to the application and save to file.
	 * @return True if changes require a restart.
	 */
	auto apply() -> bool;

	/// Local working copy of the parameters.
	core::AppParams m_localParams;
	/// Renderer value at the time the dialog was opened.
	renderer::RenderAPI::Type m_originalRenderer{};
	/// Sound value at the time the dialog was opened.
	sound::SoundAPI::Type m_originalSound{};
	/// Whether the restart warning should be displayed.
	bool m_showRestartWarning = false;
	/// Whether the popup should be opened on the next render call.
	bool m_pendingOpen = false;
	/// Whether the popup should be closed after the restart warning is dismissed.
	bool m_pendingClose = false;
};
}// namespace owl::nest::panel
