/**
 * @file ID.h
 * @author Silmaen
 * @date 14/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/UUID.h"

namespace owl::scene::component {

/**
 * @brief Class ID
 */
struct OWL_API ID {
	/// The ID
	core::UUID id;
};

}// namespace component
