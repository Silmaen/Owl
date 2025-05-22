/**
 * @file Settings.h
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "BasePanel.h"

namespace drone::panels {

/**
 * @brief Class Settings
 */
class Settings final : public BasePanel {
public:
	/**
	 * @brief Constructor.
	 */
	Settings();

	/**
	 * @brief Destructor.
	 */
	~Settings() override;

	/**
	 * @brief Copy constructor.
	 */
	Settings(const Settings&) = default;

	/**
	 * @brief Move constructor.
	 */
	Settings(Settings&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const Settings&) -> Settings& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(Settings&&) -> Settings& = default;

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
