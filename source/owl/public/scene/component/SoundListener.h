/**
 * @file SoundListener.h
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"

namespace owl::scene::component {
/**
 * @brief
 *  Sound listener component (marks the "ear" entity).
 */
struct OWL_API SoundListener {
	/// Whether this is the primary listener.
	bool primary = true;
	/// World position at the previous frame, for the Doppler velocity (not serialized).
	math::vec3d runtimeLastPosition{0.0, 0.0, 0.0};
	/// Whether runtimeLastPosition holds a position (not serialized).
	bool runtimeHasLastPosition = false;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Sound Listener"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "SoundListener"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
