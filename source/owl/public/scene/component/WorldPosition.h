/**
 * @file WorldPosition.h
 * @author Silmaen
 * @date 26/10/2026
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
 *  Position of a root entity in the world, in double precision: millimetre-precise millions of kilometres away.
 *
 * The float `Transform` of the entity stays its position relative to the scene's world origin
 * (`Scene::getWorldOrigin()`), the one the renderer and the physics see. The `owl.floating_origin` system keeps both
 * in step: a `Transform` moved by gameplay updates `position`, an edited `position` moves the `Transform`, and a shift
 * of the origin places the entity again from `position`, without the rounding of its float translation. Only root
 * entities carry a world position; a child follows its parent.
 */
struct OWL_API WorldPosition {
	/// Position in the world, in world units (metres), double precision.
	math::vec3d position{0.0, 0.0, 0.0};

	/// Runtime: `position` and the `Transform` translation were synchronised once.
	bool m_synced = false;
	/// Runtime: `Transform` translation at the last synchronisation.
	math::vec3 m_syncedLocal{0.f, 0.f, 0.f};
	/// Runtime: `position` at the last synchronisation.
	math::vec3d m_syncedPosition{0.0, 0.0, 0.0};

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "World Position"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "WorldPosition"; }

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
