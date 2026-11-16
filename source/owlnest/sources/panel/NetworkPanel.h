/**
 * @file NetworkPanel.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <network/ConditionedTransport.h>

#include <cstdint>
#include <string>

namespace owl::app {
class EngineContext;
}// namespace owl::app

namespace owl::nest::panel {

/**
 * @brief
 *  The **Network** panel: the session of the Play (host, join, leave), the simulated link conditions, the figures and
 *  the players.
 */
class NetworkPanel final {
public:
	NetworkPanel(const NetworkPanel&) = delete;
	NetworkPanel(NetworkPanel&&) = delete;
	auto operator=(const NetworkPanel&) -> NetworkPanel& = delete;
	auto operator=(NetworkPanel&&) -> NetworkPanel& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	NetworkPanel() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~NetworkPanel() = default;

	/**
	 * @brief
	 *  Show or hide the panel.
	 */
	void toggle() {
		m_visible = !m_visible;
		m_focus = m_visible;
	}

	/**
	 * @brief
	 *  Whether the panel is shown.
	 * @return True when shown.
	 */
	[[nodiscard]] auto isVisible() const -> bool { return m_visible; }

	/**
	 * @brief
	 *  Simulated conditions given to every session of the editor.
	 * @return The conditions.
	 */
	[[nodiscard]] auto getConditions() const -> const network::LinkConditions& { return m_conditions; }

	/**
	 * @brief
	 *  Give the session of the context the simulated conditions, then draw the panel when shown.
	 * @param[in,out] ioContext The engine context holding the session.
	 */
	void onImGuiRender(app::EngineContext& ioContext);

private:
	/**
	 * @brief
	 *  Draw the role of this machine and the host, join and leave gestures.
	 * @param[in,out] ioContext The engine context holding the session.
	 */
	void drawSession(app::EngineContext& ioContext);

	/**
	 * @brief
	 *  Draw the simulated link conditions.
	 */
	void drawConditions();

	/**
	 * @brief
	 *  Draw the figures and the players of the session.
	 * @param[in] iContext The engine context holding the session.
	 */
	static void drawFigures(app::EngineContext& iContext);

	/// Conditions of the outgoing messages of every session.
	network::LinkConditions m_conditions;
	/// Port to host on, 0 for one the system chooses.
	int m_hostPort{7777};
	/// Address to join.
	std::string m_joinAddress{"127.0.0.1"};
	/// Port to join.
	int m_joinPort{7777};
	/// Whether the panel is shown.
	bool m_visible{false};
	/// Whether the panel takes the focus on its next frame.
	bool m_focus{false};
};

}// namespace owl::nest::panel
