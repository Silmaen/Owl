/**
 * @file Tag.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/yaml.h"
#include "core/Core.h"

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
	 * @param ioOut The YAML context.
	 */
	void serialize(YAML::Emitter& ioOut) const {
		ioOut << YAML::Key << key();
		ioOut << YAML::BeginMap;// Tag
		ioOut << YAML::Key << "tag" << YAML::Value << tag;
		ioOut << YAML::EndMap;// Tag
	}

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const YAML::Node& iNode) {
		if (iNode["tag"])
			tag = iNode["tag"].as<std::string>();
	}
};

}// namespace owl::scene::component
