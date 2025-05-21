/**
 * @file BaseGauge.h
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <owl.h>

namespace drone::panels::gauge {

/**
 * @brief Class BaseGauge
 */
class BaseGauge {
public:
	/**
	 * @brief Constructor.
	 */
	BaseGauge();
	/**
	 * @brief Destructor.
	 */
	virtual ~BaseGauge();
	/**
	 * @brief Copy constructor.
	 */
	BaseGauge(const BaseGauge&) = default;
	/**
	 * @brief Move constructor.
	 */
	BaseGauge(BaseGauge&&) = default;
	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const BaseGauge&) -> BaseGauge& = default;
	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(BaseGauge&&) -> BaseGauge& = default;

	/**
	 * @brief Draw the cover of the gauge.
	 */
	void drawCover() const;

	/**
	 * @brief Draw the  gauge background.
	 */
	virtual void drawBack() = 0;

	/**
	 * @brief Draw the gauge cursors.
	 */
	virtual void drawCursors() = 0;

	/**
	 * @brief Define the scale of the gauge
	 * @param iScale New gauge's scale.
	 */
	void setScale(const owl::math::vec3& iScale) { m_scale = iScale; }

	/**
	 * @brief Read the gauge's scale.
	 * @return The gauge's scale.
	 */
	[[nodiscard]] auto getScale() const -> const owl::math::vec3& { return m_scale; }

	/**
	 * @brief Define the position of the gauge
	 * @param iPosition New gauge's position.
	 */
	void setPosition(const owl::math::vec3& iPosition) { m_position = iPosition; }

	/**
	 * @brief Read the gauge's position.
	 * @return The gauge's position.
	 */
	[[nodiscard]] auto getPosition() const -> const owl::math::vec3& { return m_position; }


private:
	/// The front texture
	owl::shared<owl::renderer::Texture> m_coverTexture;
	/// the object transformation
	owl::math::vec3 m_position;
	owl::math::vec3 m_scale;
};

}// namespace drone::panels::gauge
