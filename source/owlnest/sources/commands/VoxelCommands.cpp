/**
 * @file VoxelCommands.cpp
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "VoxelCommands.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/VoxelWorld.h>

#include <ranges>

namespace owl::nest::commands {

VoxelEditCommand::VoxelEditCommand(const core::UUID iEntityUuid, std::vector<VoxelBlockEdit> iEdits,
								   std::string iDescription)
	: m_entityUuid{iEntityUuid}, m_edits{std::move(iEdits)}, m_description{std::move(iDescription)} {
	m_selectAfterUndo = m_entityUuid;
	m_selectAfterRedo = m_entityUuid;
}

void VoxelEditCommand::apply(scene::Scene& ioScene, const bool iUseAfter) const {
	const auto entity = ioScene.findEntityByUUID(m_entityUuid);
	if (!entity || !entity.hasComponent<scene::component::VoxelWorld>())
		return;
	auto& voxelWorld = entity.getComponent<scene::component::VoxelWorld>().world;
	const auto applyOne = [&](const VoxelBlockEdit& iEdit) -> void {
		voxelWorld.setBlock(iEdit.coord, iUseAfter ? iEdit.after : iEdit.before);
		voxelWorld.markNeighborChunksDirty(iEdit.coord);
	};
	if (iUseAfter) {
		for (const auto& edit: m_edits) applyOne(edit);
	} else {
		for (const auto& edit: std::views::reverse(m_edits)) applyOne(edit);
	}
}

void VoxelEditCommand::undo(scene::Scene& ioScene) { apply(ioScene, /*iUseAfter=*/false); }

void VoxelEditCommand::redo(scene::Scene& ioScene) { apply(ioScene, /*iUseAfter=*/true); }

auto VoxelEditCommand::description() const -> std::string { return m_description; }

auto VoxelEditCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const VoxelEditCommand*>(&iOther);
	if (other == nullptr || other->m_entityUuid != m_entityUuid)
		return false;
	m_edits.insert(m_edits.end(), other->m_edits.begin(), other->m_edits.end());
	return true;
}

auto VoxelEditCommand::typeId() const -> size_t {
	const size_t typeHash = std::hash<std::string_view>{}("VoxelEdit");
	return typeHash ^ std::hash<uint64_t>{}(static_cast<uint64_t>(m_entityUuid));
}

}// namespace owl::nest::commands
