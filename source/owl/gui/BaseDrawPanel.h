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
 * @brief Class BaseDrawPanel.
 */
class OWL_API BaseDrawPanel {
public:
	/**
	 * @brief Default constructor.
	 */
	BaseDrawPanel();
	/**
	 * @brief Default destructor.
	 */
	virtual ~BaseDrawPanel();
	/**
	 * @brief Default copy constructor.
	 */
	BaseDrawPanel(const BaseDrawPanel &) = default;
	/**
	 * @brief Default move constructor.
	 */
	BaseDrawPanel(BaseDrawPanel &&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	BaseDrawPanel &operator=(const BaseDrawPanel &) = default;
	/**
	 * @brief Default move affectation operator.
	 */
	BaseDrawPanel &operator=(BaseDrawPanel &&) = default;
	/**
	 * @brief Update panel Status.
	 * @param iTimeStep The Time delta of the frame.
	 */
	virtual void onUpdate(const core::Timestep &iTimeStep);

	/**
	 * @brief Do the rendering.
	 */
	virtual void onRender();

	/**
	 * @brief Defines the name of the panel.
	 * @param iName The Panel's Name
	 */
	void setName(const std::string &iName) { m_panelName = iName; }
	/**
	 * @brief Get the panel's name.
	 * @return The panel's name.
	 */
	[[nodiscard]] const std::string &getName() const { return m_panelName; }

protected:
	/// the unique panel name
	std::string m_panelName;
	/// the frame buffer for this panel.
	shared<renderer::Framebuffer> mp_framebuffer;
	/// Size of the viewport
	math::FrameSize m_viewportSize = {0, 0};
	/// The camera
	shared<renderer::CameraOrtho> m_camera;
	/// View port bounds
	math::vec2 m_viewportLower = {0.0f, 0.0f};
	math::vec2 m_viewportUpper = {0.0f, 0.0f};
	bool m_viewportFocused = false;
	bool m_viewportHovered = false;
};

}// namespace owl::gui
