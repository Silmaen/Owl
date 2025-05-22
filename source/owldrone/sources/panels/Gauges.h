/**
 * @file Gauges.h
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "BasePanel.h"
#include "gauge/BaseGauge.h"

namespace drone::panels {
/**
 * @brief Class Gauges
 */
class Gauges final : public BasePanel {
public:
	/**
	 * @brief Constructor.
	 */
	Gauges();

	/**
	 * @brief Destructor.
	 */
	~Gauges() override;

	/**
	 * @brief Copy constructor.
	 */
	Gauges(const Gauges&) = default;

	/**
	 * @brief Move constructor.
	 */
	Gauges(Gauges&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const Gauges&) -> Gauges& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(Gauges&&) -> Gauges& = default;

	/**
	 * @brief Update panel Status.
	 * @param iTimeStep The Time delta of the frame.
	 */
	void onUpdate(const owl::core::Timestep& iTimeStep) override;

	/**
	 * @brief Do the rendering.
	 */
	void onRender() override;

private:
	/// the frame buffer for this panel.
	owl::shared<owl::renderer::Framebuffer> m_framebuffer;
	/// Size of the viewport
	owl::math::vec2ui m_viewportSize = {0, 0};
	/// View port bounds
	owl::math::vec2 m_viewportLower = {0.0f, 0.0f};
	owl::math::vec2 m_viewportUpper = {0.0f, 0.0f};

	bool m_viewportFocused = false;
	bool m_viewportHovered = false;

	// The gauges...
	std::vector<owl::shared<gauge::BaseGauge>> m_gauges;

	/// The camera
	owl::shared<owl::renderer::CameraOrtho> m_camera;
};
}// namespace drone::panels
