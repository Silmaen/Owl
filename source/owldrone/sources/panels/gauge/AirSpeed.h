/**
 * @file AirSpeed.h
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "BaseGauge.h"

namespace drone::panels::gauge {
/**
 * @brief Class AirSpeed
 */
class AirSpeed final : public BaseGauge {
public:
	/**
	 * @brief Constructor.
	 */
	AirSpeed();
	/**
	 * @brief Destructor.
	 */
	~AirSpeed() override;
	/**
	 * @brief Copy constructor.
	 */
	AirSpeed(const AirSpeed&) = default;
	/**
	 * @brief Move constructor.
	 */
	AirSpeed(AirSpeed&&) = default;
	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const AirSpeed&) -> AirSpeed& = default;
	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(AirSpeed&&) -> AirSpeed& = default;

	/**
	 * @brief Draw the  gauge back ground.
	 */
	void drawBack() override;

	/**
	 * @brief Draw the gauge cursors.
	 */
	void drawCursors() override;

	/**
	 * @brief Define the velocity.
	 * @param vel The new velocity from UAV.
	 */
	void setVelocity(float vel) { m_velocity = vel; }

private:
	float m_velocity = 0.f;

	[[nodiscard]] auto velocityToAngle() const -> float;

	owl::shared<owl::renderer::Texture> m_background = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor = nullptr;
};
}// namespace drone::panels::gauge
