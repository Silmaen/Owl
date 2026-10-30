/**
 * @file StreamingSource.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"
#include "core/Serializer.h"

namespace owl::scene::component {

/**
 * @brief
 *  An entity the world partition streams cells around (the player, a vehicle): the cells within the load radius of
 *  any enabled source are loaded. A scene without a source streams around its primary camera.
 */
struct OWL_API StreamingSource {
	/// Whether the source streams cells.
	bool enabled = true;
	/// Factor on the load and unload radii of the scene for this source.
	float radiusScale = 1.f;

	/// Smallest radius factor.
	static constexpr float minRadiusScale = 0.01f;
	/// Largest radius factor.
	static constexpr float maxRadiusScale = 100.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Streaming Source"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "StreamingSource"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
