/**
 * @file VoxelStructure.h
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/voxel/Block.h"
#include "math/vectors.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::voxel {

class VoxelWorld;
class BlockRegistry;

/**
 * @brief
 *  A finite, reusable block template (tree, building, …) that can be stamped into a `VoxelWorld`.
 *
 * Holds a dense `size.x * size.y * size.z` grid of `BlockId` (x fastest, then y,
 * then z), independent of any world placement. Serializes to a compact YAML
 * document (`.owlvoxstruct`): a `Size` triple plus run-length-encoded `Blocks`.
 * Air cells are kept in the grid but skipped when stamping, so a structure is
 * additive (it never erases the world around it).
 */
struct OWL_API VoxelStructure {
	/// Grid dimensions in blocks (each axis must be > 0 for a non-empty structure).
	math::vec3i size{0, 0, 0};
	/// Dense block grid of `size` volume, indexed x-fastest then y then z.
	std::vector<BlockId> blocks;
	/// Dense packed-metadata grid parallel to `blocks` (empty or shorter means all-default metadata).
	std::vector<PackedMeta> meta;

	/**
	 * @brief
	 *  Whether the structure holds no grid.
	 * @return True when any dimension is non-positive or the grid is empty.
	 */
	[[nodiscard]] auto isEmpty() const -> bool {
		return size.x() <= 0 || size.y() <= 0 || size.z() <= 0 || blocks.empty();
	}

	/**
	 * @brief
	 *  Total cell count of the grid.
	 * @return `size.x * size.y * size.z` (0 when empty).
	 */
	[[nodiscard]] auto volume() const -> size_t;

	/**
	 * @brief
	 *  Read the block at local grid coordinates (out-of-range reads return air).
	 * @param[in] iX Local x in `[0, size.x)`.
	 * @param[in] iY Local y in `[0, size.y)`.
	 * @param[in] iZ Local z in `[0, size.z)`.
	 * @return The stored block id, or `g_AirBlock` if out of range.
	 */
	[[nodiscard]] auto at(int32_t iX, int32_t iY, int32_t iZ) const -> BlockId;

	/**
	 * @brief
	 *  Read the packed metadata at local grid coordinates (out-of-range reads return default).
	 * @param[in] iX Local x in `[0, size.x)`.
	 * @param[in] iY Local y in `[0, size.y)`.
	 * @param[in] iZ Local z in `[0, size.z)`.
	 * @return The stored packed metadata, or `g_DefaultMeta` if out of range.
	 */
	[[nodiscard]] auto metaAt(int32_t iX, int32_t iY, int32_t iZ) const -> PackedMeta;

	/**
	 * @brief
	 *  Visit every non-air cell of the structure.
	 * @param[in] iVisitor Callback receiving the local grid coordinate, block id and packed metadata.
	 */
	void forEachSolid(const std::function<void(const math::vec3i&, BlockId, PackedMeta)>& iVisitor) const;

	/**
	 * @brief
	 *  Serialize the structure to a YAML string.
	 * @param[in] iName Optional display name written under the `Structure:` key.
	 * @return The YAML document.
	 */
	[[nodiscard]] auto serializeToString(std::string_view iName = "") const -> std::string;

	/**
	 * @brief
	 *  Populate the structure from a YAML string (cleared on malformed input).
	 * @param[in] iYaml The YAML document produced by `serializeToString`.
	 * @return True on success, false if malformed.
	 */
	auto deserializeFromString(std::string_view iYaml) -> bool;

	/**
	 * @brief
	 *  Capture the axis-aligned bounding box of every non-air block of a world into a structure.
	 * @param[in] iWorld The world to capture from.
	 * @param[in] iRegistry The registry resolving which ids are air.
	 * @return The captured structure (empty if the world has no solid block).
	 */
	[[nodiscard]] static auto captureFromWorld(const VoxelWorld& iWorld, const BlockRegistry& iRegistry)
			-> VoxelStructure;

	/**
	 * @brief
	 *  Stamp the structure's non-air blocks into a world with its min corner at an origin.
	 * @param[in,out] ioWorld The world to write into.
	 * @param[in] iOrigin World coordinate the structure's local `(0,0,0)` maps to.
	 */
	void stampInto(VoxelWorld& ioWorld, const math::vec3i& iOrigin) const;
};

}// namespace owl::data::voxel
