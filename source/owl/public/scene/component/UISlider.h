/**
 * @file UISlider.h
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
 * @brief UI slider widget component.
 *
 * A draggable slider with configurable range and Lua callback.
 */
struct OWL_API UISlider {
	/// Current value.
	float value = 0.f;
	/// Minimum value.
	float minValue = 0.f;
	/// Maximum value.
	float maxValue = 1.f;
	/// Track colour.
	math::vec4 trackColor{0.2f, 0.2f, 0.2f, 1.f};
	/// Fill colour.
	math::vec4 fillColor{0.4f, 0.6f, 1.f, 1.f};
	/// Handle colour.
	math::vec4 handleColor{1.f, 1.f, 1.f, 1.f};
	/// Lua callback name (called when value changes).
	std::string onValueChangedCallback;

	/// @brief Get the normalized value (0..1).
	[[nodiscard]] auto getNormalized() const -> float {
		if (maxValue <= minValue)
			return 0.f;
		return (value - minValue) / (maxValue - minValue);
	}

	/// @brief Get the class title.
	static auto name() -> const char* { return "UI Slider"; }
	/// @brief Get the YAML key.
	static auto key() -> const char* { return "UISlider"; }

	/// @brief Write this component to a YAML context.
	void serialize(const core::Serializer& iOut) const;
	/// @brief Read this component from YAML node.
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
