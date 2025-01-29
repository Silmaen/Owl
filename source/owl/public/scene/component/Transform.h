/**
 * @file Transform.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/Transform.h"

namespace owl::scene::component {

/**
 * @brief The transformation component.
 */
struct OWL_API Transform {
	/// The transformation.
	math::Transform transform;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Transform"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Transform"; }

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
