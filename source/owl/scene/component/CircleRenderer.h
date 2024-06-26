/**
 * @file CircleRenderer.h
 * @author Silmaen
 * @date 14/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::scene::component {

/**
 * @brief Struct CircleRenderer.
 */
struct OWL_API CircleRenderer {
	/// The circle color.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// The circle thickness.
	float thickness = 1.0f;
	/// The circle fading.
	float fade = 0.005f;
};

}// namespace owl::scene::component
