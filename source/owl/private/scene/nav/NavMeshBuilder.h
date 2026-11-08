/**
 * @file NavMeshBuilder.h
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/expected.h"
#include "math/vectors.h"
#include "scene/nav/NavMesh.h"

#include <cstdint>
#include <vector>

namespace owl::scene::nav {

/**
 * @brief
 *  The triangles cut from a geometry: welded corners and three indices per triangle.
 */
struct NavBuildOutput {
	/// Corners, in world space.
	std::vector<math::vec3> vertices;
	/// Three indices per triangle, counter-clockwise in the plane (x, z).
	std::vector<uint32_t> indices;
};

/**
 * @brief
 *  Cut the walkable surface of a geometry into triangles (voxels, filters, erosion, monotone regions, contours).
 * @param[in] iGeometry The triangles of the scene.
 * @param[in] iSettings How the surface is cut.
 * @param[in] iBoundsMin Lowest corner of the box the surface is cut in.
 * @param[in] iBoundsMax Highest corner of the box.
 * @return The triangles, or the cause of the failure.
 */
auto buildNavTriangles(const NavGeometry& iGeometry, const NavMeshSettings& iSettings, const math::vec3& iBoundsMin,
					   const math::vec3& iBoundsMax) -> owl::expected<NavBuildOutput, NavBuildError>;

}// namespace owl::scene::nav
