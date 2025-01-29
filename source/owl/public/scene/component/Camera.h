/**
 * @file Camera.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/YamlSerializers.h"
#include "scene/SceneCamera.h"

namespace owl::scene::component {

/**
 * @brief Struct for scene camera component.
 */
struct OWL_API Camera {
	/// If camera is the primary one.
	bool primary = true;
	/// Has camera fixed ratio.
	bool fixedAspectRatio = false;
	/// The scene camera.
	SceneCamera camera;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Camera"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Camera"; }

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
