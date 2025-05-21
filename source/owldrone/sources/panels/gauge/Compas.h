/**
 * @file Compas.h
 * @author Silmaen
 * @date 24/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "BaseGauge.h"

namespace drone::panels::gauge {

/**
 * @brief Class Compas
 */
class Compas final : public BaseGauge {
public:
	/**
	 * @brief Constructor.
	 */
	Compas();

	/**
	 * @brief Destructor.
	 */
	~Compas() override;

	/**
	 * @brief Copy constructor.
	 */
	Compas(const Compas&) = default;

	/**
	 * @brief Move constructor.
	 */
	Compas(Compas&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const Compas&) -> Compas& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(Compas&&) -> Compas& = default;

	/**
	 * @brief Draw the gauge background.
	 */
	void drawBack() override;

	/**
	 * @brief Draw the gauge cursors.
	 */
	void drawCursors() override;

	/**
	 * @brief Defines the heading.
	 * @param iHeading The heading.
	 */
	void setHeading(const float iHeading) { m_heading = iHeading; }

private:
	float m_heading = 0;
	auto headingToAngle() -> float;

	owl::shared<owl::renderer::Texture> background = nullptr;
	owl::shared<owl::renderer::Texture> cursor = nullptr;
};

}// namespace drone::panels::gauge
