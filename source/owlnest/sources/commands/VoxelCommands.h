/**
 * @file VoxelCommands.h
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoCommand.h"

#include <data/voxel/Block.h>
#include <math/vectors.h>

#include <vector>

namespace owl::nest::commands {

/**
 * @brief
 *  One block change inside a `VoxelWorld`: the block id before and after at a world coordinate.
 */
struct VoxelBlockEdit {
	/// World-grid coordinates of the changed block.
	math::vec3i coord;
	/// Block id before the edit (restored on undo).
	data::voxel::BlockId before = 0;
	/// Block id after the edit (applied on redo).
	data::voxel::BlockId after = 0;
	/// Packed metadata before the edit (restored on undo).
	data::voxel::PackedMeta beforeMeta = data::voxel::g_DefaultMeta;
	/// Packed metadata after the edit (applied on redo).
	data::voxel::PackedMeta afterMeta = data::voxel::g_DefaultMeta;
};

/**
 * @brief
 *  Undoable set of block edits on one `VoxelWorld` component (brush strokes, structure stamping).
 *
 * Stores a per-block delta so undo / redo never snapshot the whole (possibly
 * huge) world. Rapid edits on the same world coalesce through `mergeWith`, so a
 * continuous brush stroke collapses to a single undo step.
 */
class VoxelEditCommand final : public SceneUndoCommand {
public:
	/**
	 * @brief
	 *  Build a command from the entity owning the voxel world and the block deltas it applies.
	 * @param[in] iEntityUuid UUID of the entity carrying the target `VoxelWorld`.
	 * @param[in] iEdits The block deltas (already-known before / after ids).
	 * @param[in] iDescription Human-readable description for menus.
	 */
	VoxelEditCommand(core::UUID iEntityUuid, std::vector<VoxelBlockEdit> iEdits, std::string iDescription);

	/**
	 * @brief
	 *  Restore every edited block to its `before` id (in reverse application order).
	 * @param[in,out] ioScene The scene holding the voxel world.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Apply every edited block's `after` id.
	 * @param[in,out] ioScene The scene holding the voxel world.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Human-readable description for the Edit menu / tooltips.
	 * @return The description string.
	 */
	[[nodiscard]] auto description() const -> std::string override;

	/**
	 * @brief
	 *  Coalesce a later edit on the same voxel world into this command (a continuous brush stroke).
	 * @param[in] iOther The newer command to merge.
	 * @return True if merged (same entity), false otherwise.
	 */
	[[nodiscard]] auto mergeWith(const SceneUndoCommand& iOther) -> bool override;

	/**
	 * @brief
	 *  Merge-bucket id: edits on the same voxel world share one, so strokes coalesce but distinct worlds do not.
	 * @return A stable hash of the command type and target entity.
	 */
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	/**
	 * @brief
	 *  Apply the edits: the `after` ids in order when `iUseAfter`, else the `before` ids in reverse.
	 * @param[in,out] ioScene The scene holding the target voxel world.
	 * @param[in] iUseAfter Whether to write the post-edit ids (redo) or the pre-edit ids (undo).
	 */
	void apply(scene::Scene& ioScene, bool iUseAfter) const;
	/// UUID of the entity whose `VoxelWorld` is edited.
	core::UUID m_entityUuid;
	/// Block deltas, in application order.
	std::vector<VoxelBlockEdit> m_edits;
	/// Human-readable description.
	std::string m_description;
};

}// namespace owl::nest::commands
