/**
 * @file Altitude.h
 * @author Silmaen
 * @date 23/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "BaseGauge.h"

namespace drone::panels::gauge {
/**
 * @brief Class Altitude
 */
class Altitude final : public BaseGauge {
public:
	/**
	 * @brief Constructor.
	 */
	Altitude();

	/**
	 * @brief Destructor.
	 */
	~Altitude() override;

	/**
	 * @brief Copy constructor.
	 */
	Altitude(const Altitude&) = default;

	/**
	 * @brief Move constructor.
	 */
	Altitude(Altitude&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const Altitude&) -> Altitude& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(Altitude&&) -> Altitude& = default;

	/**
	 * @brief Draw the  gauge back ground.
	 */
	void drawBack() override;

	/**
	 * @brief Draw the gauge cursors.
	 */
	void drawCursors() override;

	/**
	 * @brief Define the m_altitude.
	 * @param alt The new m_altitude from UAV.
	 */
	void setAltitude(float alt) { m_altitude = alt; }

private:
	float m_altitude = 0.f;

	[[nodiscard]] auto altitudeToAngle(float iDivider = 1.0) const -> float;

	owl::shared<owl::renderer::Texture> m_background = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor10 = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor100 = nullptr;
};
}// namespace drone::panels::gauge
