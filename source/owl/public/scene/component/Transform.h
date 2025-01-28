/**
 * @file Transform.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/Transform.h"
#include "math/YamlSerializers.h"

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
	 * @param ioOut The YAML context.
	 */
	void serialize(YAML::Emitter& ioOut) const {
		ioOut << YAML::Key << key();
		ioOut << YAML::BeginMap;// Transform
		ioOut << YAML::Key << "translation" << YAML::Value << transform.translation();
		ioOut << YAML::Key << "rotation" << YAML::Value << transform.rotation();
		ioOut << YAML::Key << "scale" << YAML::Value << transform.scale();
		ioOut << YAML::EndMap;// Transform
	}

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const YAML::Node& iNode) {
		if (iNode["translation"])
			transform.translation() = iNode["translation"].as<math::vec3>();
		if (iNode["rotation"])
			transform.rotation() = iNode["rotation"].as<math::vec3>();
		if (iNode["scale"])
			transform.scale() = iNode["scale"].as<math::vec3>();
	}
};

}// namespace owl::scene::component
