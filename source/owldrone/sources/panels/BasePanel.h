/**
 * @file BasePanel.h
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "controller/UAVState.h"
#include <owl.h>

namespace drone::panels {

/**
 * @brief Class BasePanel
 */
class BasePanel {
public:
	/**
	 * @brief Constructor.
	 */
	BasePanel();

	/**
	 * @brief Destructor.
	 */
	virtual ~BasePanel();

	/**
	 * @brief Copy constructor.
	 */
	BasePanel(const BasePanel &) = default;

	/**
	 * @brief Move constructor.
	 */
	BasePanel(BasePanel &&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	BasePanel &operator=(const BasePanel &) = default;

	/**
	 * @brief Move affectation operator.
	 */
	BasePanel &operator=(BasePanel &&) = default;

	/**
	 * @brief Update panel Status.
	 * @param ts The Time delta of the frame.
	 */
	virtual void onUpdate(const owl::core::Timestep &ts) = 0;

	/**
	 * @brief Do the rendering.
	 */
	virtual void onRender() = 0;

	/**
	 * @brief Define the remote controller.
	 * @param remote The new remote controller.
	 */
	void setRemoteController(const owl::shared<controller::UAVState> &remote) { rc = remote; }

	/**
	 * @brief Access to the remote controller.
	 * @return The remote controller.
	 */
	[[nodiscard]] const owl::shared<controller::UAVState> &getRemoteController() const { return rc; }

private:
	/// pointer to the remote controller
	owl::shared<controller::UAVState> rc = nullptr;
};

}// namespace drone::panels
