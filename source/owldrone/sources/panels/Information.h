/**
 * @file Information.h
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "BasePanel.h"

namespace drone::panels {

/**
 * @brief Class Information
 */
class Information final : public BasePanel {
public:
	/**
	 * @brief Constructor.
	 */
	Information();

	/**
	 * @brief Destructor.
	 */
	~Information() override;

	/**
	 * @brief Copy constructor.
	 */
	Information(const Information&) = default;

	/**
	 * @brief Move constructor.
	 */
	Information(Information&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const Information&) -> Information& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(Information&&) -> Information& = default;

	/**
	 * @brief Update panel Status.
	 * @param iTimeStep The Time delta of the frame.
	 */
	void onUpdate(const owl::core::Timestep& iTimeStep) override;

	/**
	 * @brief Do the rendering.
	 */
	void onRender() override;

};

}// namespace drone::panels
