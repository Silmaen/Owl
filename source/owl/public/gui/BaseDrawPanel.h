/**
 * @file BaseDrawPanel.h
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Timestep.h"
#include "renderer/CameraOrtho.h"
#include "renderer/Framebuffer.h"

namespace owl::gui {

/**
 * @brief Class describing a basic panel with a draw frame buffer.
 */
class OWL_API BaseDrawPanel {
public:
	/**
	 * @brief Default constructor.
	 */
	BaseDrawPanel(std::string&& iName);
	/**
	 * @brief Default destructor.
	 */
	virtual ~BaseDrawPanel();
	/**
	 * @brief Default copy constructor.
	 */
	BaseDrawPanel(const BaseDrawPanel&) = default;
	/**
	 * @brief Default move constructor.
	 */
	BaseDrawPanel(BaseDrawPanel&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const BaseDrawPanel&) -> BaseDrawPanel& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(BaseDrawPanel&&) -> BaseDrawPanel& = default;
	/**
	 * @brief Update panel Status.
	 * @param iTimeStep The Time delta of the frame.
	 */
	virtual void onUpdate(const core::Timestep& iTimeStep);

	/**
	 * @brief Do the rendering.
	 */
	virtual void onRender();

	/**
	 * @brief Get the panel's name.
	 * @return The panel's name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_panelName; }

	/**
	 * @brief Get the viewport's size.
	 * @return The viewport's size.
	 */
	[[nodiscard]] auto getSize() const -> const math::vec2ui& { return m_viewportSize; }

protected:
	/// The unique panel name.
	std::string m_panelName;
	/// The frame buffer for this panel.
	shared<renderer::Framebuffer> mp_framebuffer;
	/// Size of the viewport.
	math::vec2ui m_viewportSize = {0, 0};
	/// The camera.
	shared<renderer::CameraOrtho> m_camera;
	/// View port bounds.
	math::vec2 m_viewportLower = {0.0f, 0.0f};
	math::vec2 m_viewportUpper = {0.0f, 0.0f};
	/// Focused state.
	bool m_viewportFocused = false;
	/// Hovered state.
	bool m_viewportHovered = false;
};

}// namespace owl::gui
