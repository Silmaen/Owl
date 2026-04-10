/**
 * @file UIProgressBar.h
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
 * @brief UI progress bar widget component.
 *
 * Displays a filled bar representing a value between 0 and 1.
 */
struct OWL_API UIProgressBar {
	/// Current progress value (0..1).
	float value = 0.5f;
	/// Background color.
	math::vec4 backgroundColor{0.2f, 0.2f, 0.2f, 1.f};
	/// Fill color.
	math::vec4 fillColor{0.3f, 0.7f, 0.3f, 1.f};

	/// @brief Get the class title.
	static auto name() -> const char* { return "UI Progress Bar"; }
	/// @brief Get the YAML key.
	static auto key() -> const char* { return "UIProgressBar"; }

	/// @brief Write this component to a YAML context.
	void serialize(const core::Serializer& iOut) const;
	/// @brief Read this component from YAML node.
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
