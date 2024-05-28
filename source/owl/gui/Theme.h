/**
 * @file Theme.h
 * @author Silmaen
 * @date 10/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "glm/glm.hpp"

namespace owl::gui {

/**
 * @brief Definition  of the GUI theme.
 */
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
struct Theme {
	glm::vec4 groupHeader{0.184f, 0.184f, 0.184f, 1.0f};
	glm::vec4 propertyField{0.0588f, 0.0588f, 0.0588f, 1.0};
	glm::vec4 titleBar{0.0824f, 0.0824f, 0.0824f, 1.0f};
	glm::vec4 text{0.753f, 0.753f, 0.753f, 1.0};
	glm::vec4 backgroundDark{0.102f, 0.102f, 0.102f, 1.0f};
	glm::vec4 highlight{0.153f, 0.725f, 0.949f, 1.0f};
	glm::vec4 background{0.141f, 0.141f, 0.141f, 1.0f};
	glm::vec4 backgroundPopup{0.196f, 0.196f, 0.196f, 1.0f};
} OWL_ALIGN(128);
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

}// namespace owl::gui
