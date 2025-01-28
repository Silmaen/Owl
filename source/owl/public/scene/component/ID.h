/**
 * @file ID.h
 * @author Silmaen
 * @date 14/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/UUID.h"

/**
 * @brief namespace for the scene components.
 */
namespace owl::scene::component {

/**
 * @brief Class for the entity's ID.
 */
struct OWL_API ID {
	/// The ID.
	core::UUID id;
};

}// namespace owl::scene::component
