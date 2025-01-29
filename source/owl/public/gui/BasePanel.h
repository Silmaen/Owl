/**
 * @file BasePanel.h
 * @author Silmaen
 * @date 10/16/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Timestep.h"
#include "event/Event.h"
#include "math/vectors.h"

namespace owl::gui {

/**
 * @brief Class describing a basic gui panel.
 */
class OWL_API BasePanel {
public:
	/**
	 * @brief Default constructor.
	 */
	BasePanel(std::string&& iName);
	/**
	 * @brief Default destructor.
	 */
	virtual ~BasePanel();
	/**
	 * @brief Default copy constructor.
	 */
	BasePanel(const BasePanel&) = default;
	/**
	 * @brief Default move constructor.
	 */
	BasePanel(BasePanel&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const BasePanel&) -> BasePanel& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(BasePanel&&) -> BasePanel& = default;
	/**
	 * @brief Initialization when attached.
	 */
	virtual void attach() {}
	/**
	 * @brief Cleaning after detached.
	 */
	virtual void detach() {}
	/**
	 * @brief Update panel Status.
	 * @param iTimeStep The Time delta of the frame.
	 */
	virtual void onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {}
	/**
	 * @brief Action on event.
	 * @param[in,out] ioEvent The Event to react.
	 */
	virtual void onEvent([[maybe_unused]] event::Event& ioEvent) {}
	/**
	 * @brief Update internal variables and do the rendering.
	 */
	void onRender();

	/**
	 * @brief Do the rendering.
	 */
	virtual void onRenderInternal() = 0;

	/**
	 * @brief get the panel size.
	 */
	[[nodiscard]] auto getSize() const -> const math::vec2ui& { return m_size; }

	/**
	 * @brief If the panel is hovered.
	 */
	[[nodiscard]] auto isHovered() const -> bool { return m_hovered; }
	/**
	 * @brief If the panel is focused.
	 */
	[[nodiscard]] auto isFocused() const -> bool { return m_focused; }

protected:
	std::string m_name;
	/// Panel size.
	math::vec2ui m_size = {0, 0};
	/// Panel bounds.
	math::vec2 m_lower = {0.0f, 0.0f};
	math::vec2 m_upper = {0.0f, 0.0f};
	/// Focused state.
	bool m_focused = false;
	/// Hovered state.
	bool m_hovered = false;
};

}// namespace owl::gui
