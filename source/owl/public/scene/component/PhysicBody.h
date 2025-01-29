/**
 * @file PhysicBody.h
 * @author Silmaen
 * @date 12/26/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/YamlSerializers.h"
#include "scene/SceneBody.h"

namespace owl::scene::component {

/**
 * @brief Physical body information.
 */
struct OWL_API PhysicBody {
	/// The physical body.
	SceneBody body;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Physical body"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "PhysicBody"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param ioOut The YAML context.
	 */
	void serialize(YAML::Emitter& ioOut) const;

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const YAML::Node& iNode);
};

}// namespace owl::scene::component
