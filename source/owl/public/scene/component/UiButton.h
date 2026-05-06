/**
 * @file UiButton.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

namespace owl::scene::component {
/**
 * @brief
 *  UI button widget component.
 *
 * Interactive button with visual states and Lua callback support.
 */
struct OWL_API UiButton {
	/// Button visual state (runtime only, not serialized).
	enum struct State : uint8_t { Normal, Hovered, Pressed, Disabled };
	/// Current runtime state.
	State state = State::Normal;
	/// Colour in normal state.
	math::vec4 normalColor{0.3f, 0.3f, 0.3f, 1.f};
	/// Colour when hovered.
	math::vec4 hoverColor{0.4f, 0.4f, 0.4f, 1.f};
	/// Colour when pressed.
	math::vec4 pressedColor{0.2f, 0.2f, 0.2f, 1.f};
	/// Colour when disabled.
	math::vec4 disabledColor{0.15f, 0.15f, 0.15f, 0.5f};
	/// Name of the Lua callback function (called on click).
	std::string onClickCallback;

	/**
	 * @brief
	 *  Get the current colour based on state.
	 * @return The colour matching the current `state`.
	 */
	[[nodiscard]] auto getCurrentColor() const noexcept -> math::vec4 {
		switch (state) {
			case State::Normal:
				return normalColor;
			case State::Hovered:
				return hoverColor;
			case State::Pressed:
				return pressedColor;
			case State::Disabled:
				return disabledColor;
		}
		return normalColor;
	}

	/**
	 * @brief
	 *  Get the class title.
	 * @return The display name of the component.
	 */
	static auto name() noexcept -> const char* { return "UI Button"; }

	/**
	 * @brief
	 *  Get the YAML key.
	 * @return The YAML serialization key.
	 */
	static auto key() noexcept -> const char* { return "UiButton"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The serializer used as output.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The serializer wrapping the source YAML node.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
