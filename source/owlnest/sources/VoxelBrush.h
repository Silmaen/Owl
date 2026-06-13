/**
 * @file VoxelBrush.h
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <data/voxel/Block.h>
#include <data/voxel/VoxelStructure.h>

#include <optional>
#include <string>

namespace owl::nest {

/**
 * @brief
 *  Shared voxel-brush state edited by the Voxel Palette panel and consumed by the viewport.
 *
 * When `active`, the viewport intercepts clicks over a `VoxelWorld` in Edit
 * mode: a left-click places `paintBlock` against the targeted face (or stamps
 * `structure` when one is selected, or erases when `eraser` is set), a
 * right-click always erases the targeted block.
 */
struct VoxelBrush {
	/// Whether the brush tool is engaged (intercepts viewport clicks instead of entity selection).
	bool active = false;
	/// When true, a left-click erases the targeted block instead of placing one.
	bool eraser = false;
	/// Block id placed on a left-click when not erasing / stamping.
	data::voxel::BlockId paintBlock = 1;
	/// When set, a left-click stamps this structure (its min corner at the targeted empty cell) instead of one block.
	std::optional<data::voxel::VoxelStructure> structure;
	/// Display name of the selected structure (empty when none).
	std::string structureName;
};

}// namespace owl::nest
