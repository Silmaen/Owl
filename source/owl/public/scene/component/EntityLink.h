/**
 * @file EntityLink.h
 * @author Silmaen
 * @date 1/1/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "scene/Entity.h"
#include "core/Core.h"

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
	 * @param ioOut The YAML context.
	 */
	void serialize(YAML::Emitter& ioOut) const {
		ioOut << YAML::Key << key();
		ioOut << YAML::BeginMap;// Tag
		ioOut << YAML::Key << "linkedEntityName" << YAML::Value << linkedEntityName;
		ioOut << YAML::EndMap;// Tag
	}

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const YAML::Node& iNode) {
		if (iNode["linkedEntityName"])
			linkedEntityName = iNode["linkedEntityName"].as<std::string>();
	}
	/// The linked entity.
	Entity linkedEntity;
};
}// namespace owl::scene::component
