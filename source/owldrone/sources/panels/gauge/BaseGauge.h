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
	void drawCover();

	/**
	 * @brief Draw the  gauge back ground.
	 */
	virtual void drawBack() = 0;

	/**
	 * @brief Draw the gauge cursors.
	 */
	virtual void drawCursors() = 0;

	/**
	 * @brief Define the scale of the gauge
	 * @param sc New gauge's scale.
	 */
	void setScale(const owl::math::vec2& sc) { transform.size = sc; }

	/**
	 * @brief Read the gauge's scale.
	 * @return The gauge's scale.
	 */
	[[nodiscard]] auto getScale() const -> const owl::math::vec2& { return transform.size; }

	/**
	 * @brief Define the position of the gauge
	 * @param pos New gauge's position.
	 */
	void setPosition(const owl::math::vec3& pos) { transform.position = pos; }

	/**
	 * @brief Read the gauge's position.
	 * @return The gauge's position.
	 */
	[[nodiscard]] auto getPosition() const -> const owl::math::vec3& { return transform.position; }

	/**
	 * @brief Access to the object's transformation.
	 * @return The transformation.
	 */
	[[nodiscard]] auto getTransform() const -> const owl::renderer::utils::PRS& { return transform; }


private:
	/// The front texture
	owl::shared<owl::renderer::Texture> coverTexture;
	/// the object transformation
	owl::renderer::utils::PRS transform;
};

}// namespace drone::panels::gauge
