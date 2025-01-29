/**
 * @file Tag.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"

namespace owl::scene::component {

/**
 * @brief A tag component.
 */
struct OWL_API Tag {
	/// The tag name.
	std::string tag;
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Tag"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
