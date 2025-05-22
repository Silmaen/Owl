/**
 * @file MotorRate.h
 * @author Silmaen
 * @date 24/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "BaseGauge.h"

namespace drone::panels::gauge {

/**
 * @brief Class MotorRate
 */
class MotorRate final : public BaseGauge {
public:
	/**
	 * @brief Constructor.
	 */
	MotorRate();

	/**
	 * @brief Destructor.
	 */
	~MotorRate() override;

	/**
	 * @brief Copy constructor.
	 */
	MotorRate(const MotorRate&) = default;

	/**
	 * @brief Move constructor.
	 */
	MotorRate(MotorRate&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const MotorRate&) -> MotorRate& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(MotorRate&&) -> MotorRate& = default;

	/**
	 * @brief Draw the gauge background.
	 */
	void drawBack() override;

	/**
	 * @brief Draw the gauge cursors.
	 */
	void drawCursors() override;

	/**
	 * @brief Defines the motor rates.
	 * @param iRates Motor rates.
	 */
	void setMotorRates(const std::vector<float>& iRates) { m_rates = iRates; }

private:
	std::vector<float> m_rates;

	[[nodiscard]] auto rateToAngle(uint8_t iRate) const -> float;

	owl::shared<owl::renderer::Texture> m_background = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor1 = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor2 = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor3 = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor4 = nullptr;
};

}// namespace drone::panels::gauge
