/**
 * @file EntityLink.h
 * @author Silmaen
 * @date 1/1/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "scene/Entity.h"

namespace owl::scene::component {

/**
 * @brief Component to describe link between entities.
 */
struct OWL_API EntityLink {
	/// the name of the linked entity.
	std::string linkedEntityName;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Entity Link"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "EntityLink"; }

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
	/// The linked entity.
	Entity linkedEntity;
};
}// namespace owl::scene::component
