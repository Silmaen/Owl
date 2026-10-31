/**
 * @file WalkQueries.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <optional>
#include <vector>

namespace owl::scene {
class Scene;
class Entity;
}// namespace owl::scene

namespace owl::scene::nav {

/**
 * @brief
 *  Find the shortest path across the `WalkArea` of an entity, in world space.
 * @param[in] iScene The scene.
 * @param[in] iArea The entity holding the `WalkArea`.
 * @param[in] iFrom Start, in world space (moved onto the floor when outside).
 * @param[in] iTo Goal, in world space (moved onto the floor when outside).
 * @param[out] oPath The corners of the path, start and goal included, in world space.
 * @return True when a path joins them.
 */
OWL_API auto findWalkPath(const Scene& iScene, const Entity& iArea, const math::vec2& iFrom, const math::vec2& iTo,
						  std::vector<math::vec2>& oPath) -> bool;

/**
 * @brief
 *  Find the point of the `WalkArea` of an entity nearest to a world point.
 * @param[in] iScene The scene.
 * @param[in] iArea The entity holding the `WalkArea`.
 * @param[in] iPoint The point, in world space.
 * @return The nearest point of the floor in world space, nothing when the entity has no floor.
 */
OWL_API auto getWalkClosestPoint(const Scene& iScene, const Entity& iArea, const math::vec2& iPoint)
		-> std::optional<math::vec2>;

/**
 * @brief
 *  Check if a world point is on the `WalkArea` of an entity.
 * @param[in] iScene The scene.
 * @param[in] iArea The entity holding the `WalkArea`.
 * @param[in] iPoint The point, in world space.
 * @return True when on the floor.
 */
OWL_API auto isOnWalkArea(const Scene& iScene, const Entity& iArea, const math::vec2& iPoint) -> bool;

/**
 * @brief
 *  Get the scale by depth the `WalkArea` of an entity gives at a world point.
 * @param[in] iScene The scene.
 * @param[in] iArea The entity holding the `WalkArea`.
 * @param[in] iPoint The point, in world space.
 * @return The scale (1 when the area does not scale or the entity has none).
 */
OWL_API auto getWalkScaleAt(const Scene& iScene, const Entity& iArea, const math::vec2& iPoint) -> float;

/**
 * @brief
 *  Send a `WalkAgent` to a world point of its area: the path is found now, the `owl.walk_agents` system walks it.
 * @param[in,out] ioScene The scene.
 * @param[in] iAgent The entity holding the `WalkAgent`.
 * @param[in] iTarget The goal, in world space (moved onto the floor when outside).
 * @return True when a path was found; false leaves the agent standing.
 */
OWL_API auto walkAgentTo(Scene& ioScene, const Entity& iAgent, const math::vec2& iTarget) -> bool;

}// namespace owl::scene::nav
