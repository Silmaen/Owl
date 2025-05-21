/**
 * @file Viewport.h
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "BasePanel.h"

namespace drone::panels {
/**
 * @brief Class Viewport
 */
class Viewport final: public BasePanel {
public:
	/**
	 * @brief Constructor.
	 */
	Viewport();

	/**
	 * @brief Destructor.
	 */
	~Viewport() override;

	/**
	 * @brief Copy constructor.
	 */
	Viewport(const Viewport&) = default;

	/**
	 * @brief Move constructor.
	 */
	Viewport(Viewport&&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	auto operator=(const Viewport&) -> Viewport& = default;

	/**
	 * @brief Move affectation operator.
	 */
	auto operator=(Viewport&&) -> Viewport& = default;

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
	owl::shared<owl::renderer::Framebuffer> mp_framebuffer;
	/// Size of the viewport
	owl::math::vec2ui m_viewportSize = {0, 0};
	/// View port bounds
	owl::math::vec2 m_viewportLower = {0.0f, 0.0f};
	owl::math::vec2 m_viewportUpper = {0.0f, 0.0f};

	bool m_viewportFocused = false;
	bool m_viewportHovered = false;
	/// The camera
	owl::shared<owl::renderer::CameraOrtho> m_camera;
};
}// namespace drone::panels
