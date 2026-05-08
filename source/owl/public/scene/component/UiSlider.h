/**
 * @file UiSlider.h
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
 *  UI slider widget component.
 *
 * A draggable slider with configurable range and Lua callback.
 */
struct OWL_API UiSlider {
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

	/**
	 * @brief
	 *  Get the normalized value (0..1).
	 * @return The current value mapped into the `[minValue, maxValue]` range, or 0 when the range is degenerate.
	 */
	[[nodiscard]] auto getNormalized() const noexcept -> float {
		if (maxValue <= minValue)
			return 0.f;
		return (value - minValue) / (maxValue - minValue);
	}

	/**
	 * @brief
	 *  Get the class title.
	 * @return The display name of the component.
	 */
	static auto name() noexcept -> const char* { return "UI Slider"; }

	/**
	 * @brief
	 *  Get the YAML key.
	 * @return The YAML serialization key.
	 */
	static auto key() noexcept -> const char* { return "UiSlider"; }

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
