/**
 * @file ArtificialHorizon.h
 * @author Silmaen
 * @date 24/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "BaseGauge.h"

namespace drone::panels::gauge {

/**
 * @brief Class ArtificialHorizon
 */
class ArtificialHorizon final : public BaseGauge {
public:
	/**
	 * @brief Constructor.
	 */
	ArtificialHorizon();

	/**
	 * @brief Destructor.
	 */
	~ArtificialHorizon() override;

	/**
	 * @brief Copy constructor.
	 */
	ArtificialHorizon(const ArtificialHorizon&) = default;

	/**
	 * @brief Move constructor.
	 */
	ArtificialHorizon(ArtificialHorizon&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const ArtificialHorizon&) -> ArtificialHorizon& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(ArtificialHorizon&&) -> ArtificialHorizon& = default;

	/**
	 * @brief Draw the  gauge back ground.
	 */
	void drawBack() override;

	/**
	 * @brief Draw the gauge cursors.
	 */
	void drawCursors() override;

	/**
	 * @brief Defines th pitch and roll of the gauge
	 * @param p Pitch.
	 * @param r Roll.
	 */
	void setPitchRoll(float p, float r) {
		m_pitch = p;
		m_roll = r;
	}

private:
	float m_pitch = 0;
	float m_roll = 0;

	owl::shared<owl::renderer::Texture> m_background = nullptr;
	owl::shared<owl::renderer::Texture> m_rollRing = nullptr;
	owl::shared<owl::renderer::Texture> m_olivePitch = nullptr;
	owl::shared<owl::renderer::Texture> m_cursor = nullptr;
};

}// namespace drone::panels::gauge
