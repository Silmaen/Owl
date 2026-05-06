/**
 * @file UiProgressBar.h
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
 *  UI progress bar widget component.
 *
 * Displays a filled bar representing a value between 0 and 1.
 */
struct OWL_API UiProgressBar {
	/// Current progress value (0..1).
	float value = 0.5f;
	/// Background colour.
	math::vec4 backgroundColor{0.2f, 0.2f, 0.2f, 1.f};
	/// Fill colour.
	math::vec4 fillColor{0.3f, 0.7f, 0.3f, 1.f};

	/**
	 * @brief
	 *  Get the class title.
	 * @return The display name of the component.
	 */
	static auto name() noexcept -> const char* { return "UI Progress Bar"; }

	/**
	 * @brief
	 *  Get the YAML key.
	 * @return The YAML serialization key.
	 */
	static auto key() noexcept -> const char* { return "UiProgressBar"; }

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
