/**
 * @file ViewPort.h
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include <owl.h>

namespace owl::raycaster::panel {

/**
 * @brief Class ViewPort.
 */
class ViewPort final : public gui::BaseDrawPanel {
public:
	/**
	 * @brief Default constructor.
	 */
	ViewPort();
	/**
	 * @brief Default destructor.
	 */
	~ViewPort() override;
	/**
	 * @brief Default copy constructor.
	 */
	ViewPort(const ViewPort&) = default;
	/**
	 * @brief Default move constructor.
	 */
	ViewPort(ViewPort&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const ViewPort&) -> ViewPort& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(ViewPort&&) -> ViewPort& = default;
	/**
	 * @brief Update panel Status.
	 * @param iTimeStep The Time delta of the frame.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;
};

}// namespace owl::raycaster::panel
