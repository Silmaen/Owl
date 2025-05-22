/**
 * @file BasePanel.h
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "controller/RemoteController.h"
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
	BasePanel(const BasePanel&) = default;

	/**
	 * @brief Move constructor.
	 */
	BasePanel(BasePanel&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const BasePanel&) -> BasePanel& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(BasePanel&&) -> BasePanel& = default;

	/**
	 * @brief Update panel Status.
	 * @param iTs The Time delta of the frame.
	 */
	virtual void onUpdate(const owl::core::Timestep& iTs) = 0;

	/**
	 * @brief Do the rendering.
	 */
	virtual void onRender() = 0;

	/**
	 * @brief Define the remote controller.
	 * @param iRemote The new remote controller.
	 */
	void setRemoteController(const owl::shared<controller::RemoteController>& iRemote) { m_rc = iRemote; }

	/**
	 * @brief Access to the remote controller.
	 * @return The remote controller.
	 */
	[[nodiscard]] auto getRemoteController() const -> const owl::shared<controller::RemoteController>& { return m_rc; }

private:
	/// pointer to the remote controller
	owl::shared<controller::RemoteController> m_rc = nullptr;
};

}// namespace drone::panels
