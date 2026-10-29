/**
 * @file MeshLod.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/geometry/MeshLoader.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace owl::data::geometry {

/**
 * @brief
 *  Simplify a triangle soup by vertex clustering: corners in one cell of a grid merge into the corner nearest to
 *  their mean, and the triangles that collapse are dropped.
 *
 * The grid is the finest one that keeps at most `iRatio` of the triangles. The kept corners are corners of the input,
 * so their attributes (texture coordinates, normals, skin) stay valid.
 * @param[in] iCornerPositions Positions of the corners, three per triangle.
 * @param[in] iRatio Highest ratio of the triangles kept, in `(0, 1)`.
 * @return The corners of the kept triangles (indices into `iCornerPositions`), three per triangle.
 */
[[nodiscard]] OWL_API auto simplifyCorners(std::span<const math::vec3> iCornerPositions, float iRatio)
		-> std::vector<uint32_t>;

/**
 * @brief
 *  Build the levels of detail of a mesh, each submesh simplified on its own.
 * @param[in] iMesh The mesh.
 * @param[in] iSettings The level count, the reduction and the screen size of the first level.
 * @return The levels, coarser and coarser; fewer than asked when a level cannot remove more triangles.
 */
[[nodiscard]] OWL_API auto buildMeshLods(const LoadedMesh& iMesh, const LodSettings& iSettings)
		-> std::vector<MeshLodLevel>;

/**
 * @brief
 *  Check if bytes are a cooked mesh (`OWLM` container).
 * @param[in] iData The bytes.
 * @return True when they start with the magic.
 */
[[nodiscard]] OWL_API auto isCookedMesh(std::span<const uint8_t> iData) -> bool;

/**
 * @brief
 *  Write a cooked mesh: the source file and its levels of detail.
 * @param[in] iSource The bytes of the source file.
 * @param[in] iLods The levels of detail.
 * @return The `OWLM` container.
 */
[[nodiscard]] OWL_API auto writeCookedMesh(std::span<const uint8_t> iSource, std::span<const MeshLodLevel> iLods)
		-> std::vector<uint8_t>;

/**
 * @brief
 *  A cooked mesh read back.
 */
struct OWL_API CookedMesh {
	/// The bytes of the source file (a view into the container).
	std::span<const uint8_t> source;
	/// The levels of detail.
	std::vector<MeshLodLevel> lods;
};

/**
 * @brief
 *  Read a cooked mesh (untrusted bytes: every size is checked).
 * @param[in] iData The container.
 * @return The source and the levels, nothing when the container is invalid (logged).
 */
[[nodiscard]] OWL_API auto readCookedMesh(std::span<const uint8_t> iData) -> std::optional<CookedMesh>;

}// namespace owl::data::geometry
