/**
 * @file Tag.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include <string>

namespace owl::scene::component {

struct OWL_API Tag {
	std::string tag;

	Tag() = default;
	Tag(const std::string &tag_)
		: tag(tag_) {}
};

}// namespace owl::scene::component
