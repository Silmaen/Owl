/**
 * @file Camera.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
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
	 * @brief Get the serializer key for this component
	 * @return The serializer key.
	 */
	static auto key() -> const char* { return "Camera"; }

	/**
	 * @brief Write this component to a serializer context.
	 * @param iOut The serializer context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief Read this component from serializer node.
	 * @param iNode The serializer node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
