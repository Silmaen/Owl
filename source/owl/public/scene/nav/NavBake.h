/**
 * @file NavBake.h
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"
#include "math/vectors.h"
#include "scene/nav/NavMesh.h"

#include <cstdint>
#include <vector>

namespace owl::scene {
class Scene;
class Entity;
}// namespace owl::scene

namespace owl::scene::nav {

/**
 * @brief
 *  Gather the static geometry a `NavMeshVolume` reads, in world space.
 *
 * The colliders of the static 3D bodies (a sphere or a capsule as a cylinder, a convex hull as its mesh) and the
 * meshes of the `MeshRenderer`s without a body, following the `sources` of the volume. The hidden entities and the ones
 * that move are skipped, with their children: a `NavAgent`, a `NavObstacle`, a `SplineFollower`, a body that is not
 * static. The entities are read in the order of their UUID.
 * @param[in] iScene The scene.
 * @param[in] iVolume The entity holding the `NavMeshVolume`.
 * @return The triangles (empty when the entity holds no volume).
 */
OWL_API auto collectNavGeometry(const Scene& iScene, const Entity& iVolume) -> NavGeometry;

/**
 * @brief
 *  Get the box of a `NavMeshVolume`, in world space.
 * @param[in] iScene The scene.
 * @param[in] iVolume The entity holding the volume.
 * @param[out] oMin Lowest corner.
 * @param[out] oMax Highest corner.
 */
OWL_API void getNavVolumeBounds(const Scene& iScene, const Entity& iVolume, math::vec3& oMin, math::vec3& oMax);

/**
 * @brief
 *  Bake the navigation mesh of a `NavMeshVolume` from the geometry inside its box, into its `vertices` and `indices`.
 * @param[in,out] ioScene The scene.
 * @param[in] iVolume The entity holding the volume.
 * @return Nothing on success (the volume holds the new triangles), the cause otherwise (the volume is left as it was).
 */
OWL_API auto bakeNavMesh(Scene& ioScene, const Entity& iVolume) -> owl::expected<void, NavBuildError>;

/**
 * @brief
 *  Read the triangles of the `WalkArea`s whose entity stands inside a volume (the `WalkAreas` sources), each corner
 *  (x, y) of a floor written (x, 0, y), counter-clockwise in the plane (x, z).
 * @param[in] iScene The scene.
 * @param[in] iVolume The entity of the `NavMeshVolume`.
 * @param[out] oVertices The corners.
 * @param[out] oIndices Three indices in `oVertices` per triangle.
 * @return True when a walk area gave triangles.
 */
OWL_API auto collectWalkTriangles(const Scene& iScene, const Entity& iVolume, std::vector<math::vec3>& oVertices,
								  std::vector<uint32_t>& oIndices) -> bool;

/**
 * @brief
 *  Get the `NavLink`s of a scene, in world space.
 * @param[in] iScene The scene.
 * @return The links, in the order of the UUIDs of their entities.
 */
OWL_API auto collectNavLinks(const Scene& iScene) -> std::vector<NavMeshLink>;

/**
 * @brief
 *  Find the `NavMeshVolume` whose box holds a point.
 * @param[in] iScene The scene.
 * @param[in] iPoint The point, in world space.
 * @return The entity holding the volume, an invalid entity when none holds the point.
 */
OWL_API auto findNavVolume(const Scene& iScene, const math::vec3& iPoint) -> Entity;

}// namespace owl::scene::nav
