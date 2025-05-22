/**
 * @file VerticalSpeed.h
 * @author Silmaen
 * @date 23/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "BaseGauge.h"

namespace drone::panels::gauge {
/**
 * @brief Class VerticalSpeed
 */
class VerticalSpeed final : public BaseGauge {
public:
	/**
	 * @brief Constructor.
	 */
	VerticalSpeed();

	/**
	 * @brief Destructor.
	 */
	~VerticalSpeed() override;

	/**
	 * @brief Copy constructor.
	 */
	VerticalSpeed(const VerticalSpeed&) = default;

	/**
	 * @brief Move constructor.
	 */
	VerticalSpeed(VerticalSpeed&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const VerticalSpeed&) -> VerticalSpeed& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(VerticalSpeed&&) -> VerticalSpeed& = default;

	/**
	 * @brief Draw the gauge background.
	 */
	void drawBack() override;

	/**
	 * @brief Draw the gauge cursors.
	 */
	void drawCursors() override;

	/**
	 * @brief Define the velocity.
	 * @param iVel The new velocity from UAV.
	 */
	void setVerticalVelocity(const float iVel) { m_verticalVelocity = iVel; }

private:
	float m_verticalVelocity = 0.f;

	[[nodiscard]] auto velocityToAngle() const -> float;

	owl::shared<owl::renderer::Texture> m_background = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor = nullptr;
};
}// namespace drone::panels::gauge
