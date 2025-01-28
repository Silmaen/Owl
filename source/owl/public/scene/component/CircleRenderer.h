/**
 * @file CircleRenderer.h
 * @author Silmaen
 * @date 14/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/YamlSerializers.h"

namespace owl::scene::component {

/**
 * @brief Struct for component drawing a circle.
 */
struct OWL_API CircleRenderer {
	/// The circle color.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// The circle thickness.
	float thickness = 1.0f;
	/// The circle fading.
	float fade = 0.005f;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Circle Renderer"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "CircleRenderer"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param ioOut The YAML context.
	 */
	void serialize(YAML::Emitter& ioOut) const {
		ioOut << YAML::Key << key();
		ioOut << YAML::BeginMap;// CircleRenderer
		ioOut << YAML::Key << "color" << YAML::Value << color;
		ioOut << YAML::Key << "thickness" << YAML::Value << thickness;
		ioOut << YAML::Key << "fade" << YAML::Value << fade;
		ioOut << YAML::EndMap;// CircleRenderer
	}

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const YAML::Node& iNode) {
		if (iNode["color"])
			color = iNode["color"].as<math::vec4>();
		if (iNode["thickness"])
			thickness = iNode["thickness"].as<float>();
		if (iNode["fade"])
			fade = iNode["fade"].as<float>();
	}
};

}// namespace owl::scene::component
