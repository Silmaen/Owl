/**
 * @file EntityCommands.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EntityCommands.h"

#include <scene/component/Hierarchy.h>
#include <scene/component/Transform.h>

#include <cstddef>
#include <format>

namespace owl::nest::commands {
// --- CreateEntityCommand ---

CreateEntityCommand::CreateEntityCommand(const scene::Entity& iEntity)
	: m_snapshot{EntitySnapshot::capture(iEntity)}, m_name{iEntity.getName()} {
	m_selectAfterRedo = m_snapshot.uuid;
}

CreateEntityCommand::~CreateEntityCommand() = default;

void CreateEntityCommand::undo(scene::Scene& ioScene) {
	// Re-capture before destroying (entity may have been modified since creation).
	if (auto entity = ioScene.findEntityByUUID(m_snapshot.uuid); entity) {
		m_snapshot = EntitySnapshot::capture(entity);
		ioScene.destroyEntity(entity);
	}
}

void CreateEntityCommand::redo(scene::Scene& ioScene) { m_snapshot.restore(ioScene); }

auto CreateEntityCommand::description() const -> std::string { return std::format("Create '{}'", m_name); }

// --- DeleteEntityCommand ---
DeleteEntityCommand::DeleteEntityCommand(const scene::Entity& iEntity, const scene::Scene& iScene)
	: m_snapshot{EntitySnapshot::capture(iEntity)}, m_name{iEntity.getName()},
	  m_slot{HierarchySlot::capture(iEntity, iScene)},
	  m_childrenUuids{iEntity.getComponent<scene::component::Hierarchy>().childrenIds} {
	m_selectAfterUndo = m_snapshot.uuid;
	for (const auto childUuid: m_childrenUuids) {
		const auto child = iScene.findEntityByUUID(childUuid);
		m_childrenLocal.push_back(child ? child.getComponent<scene::component::Transform>().transform
										: math::Transform{});
	}
}

DeleteEntityCommand::~DeleteEntityCommand() = default;

void DeleteEntityCommand::undo(scene::Scene& ioScene) {
	const auto entity = m_snapshot.restore(ioScene);
	if (!entity)
		return;
	m_slot.restore(entity, ioScene);
	for (size_t i = 0; i < m_childrenUuids.size(); ++i) {
		const auto child = ioScene.findEntityByUUID(m_childrenUuids[i]);
		if (!child)
			continue;
		HierarchySlot{.parentUuid = m_snapshot.uuid, .siblingIndex = i}.restore(child, ioScene);
		child.getComponent<scene::component::Transform>().transform = m_childrenLocal[i];
	}
}

void DeleteEntityCommand::redo(scene::Scene& ioScene) {
	if (auto entity = ioScene.findEntityByUUID(m_snapshot.uuid); entity) {
		// destroyEntity reparents children to grandparent automatically.
		ioScene.destroyEntity(entity);
	}
}

auto DeleteEntityCommand::description() const -> std::string { return std::format("Delete '{}'", m_name); }

// --- DeleteSubtreeCommand ---
DeleteSubtreeCommand::DeleteSubtreeCommand(const scene::Entity& iEntity, const scene::Scene& iScene)
	: m_snapshot{SubtreeSnapshot::capture(iEntity, iScene)}, m_name{iEntity.getName()},
	  m_slot{HierarchySlot::capture(iEntity, iScene)} {
	m_selectAfterUndo = iEntity.getUUID();
}

DeleteSubtreeCommand::~DeleteSubtreeCommand() = default;

void DeleteSubtreeCommand::undo(scene::Scene& ioScene) {
	if (const auto root = m_snapshot.restore(ioScene); root)
		m_slot.restore(root, ioScene);
}

void DeleteSubtreeCommand::redo(scene::Scene& ioScene) {
	if (auto entity = ioScene.findEntityByUUID(m_snapshot.entities[0].uuid); entity)
		ioScene.destroyEntityWithChildren(entity);
}

auto DeleteSubtreeCommand::description() const -> std::string {
	return std::format("Delete '{}' with children", m_name);
}

// --- DuplicateEntityCommand ---
DuplicateEntityCommand::DuplicateEntityCommand(const scene::Entity& iOriginal, const scene::Entity& iDuplicate)
	: m_duplicateSnapshot{EntitySnapshot::capture(iDuplicate)}, m_name{iOriginal.getName()} {
	m_selectAfterRedo = m_duplicateSnapshot.uuid;
}

DuplicateEntityCommand::~DuplicateEntityCommand() = default;

void DuplicateEntityCommand::undo(scene::Scene& ioScene) {
	if (auto entity = ioScene.findEntityByUUID(m_duplicateSnapshot.uuid); entity)
		ioScene.destroyEntity(entity);
}

void DuplicateEntityCommand::redo(scene::Scene& ioScene) { m_duplicateSnapshot.restore(ioScene); }

auto DuplicateEntityCommand::description() const -> std::string { return std::format("Duplicate '{}'", m_name); }

// --- DuplicateSubtreeCommand ---
DuplicateSubtreeCommand::DuplicateSubtreeCommand(const scene::Entity& iOriginal, const scene::Entity& iDuplicateRoot,
												 const scene::Scene& iScene)
	: m_duplicateSnapshot{SubtreeSnapshot::capture(iDuplicateRoot, iScene)}, m_name{iOriginal.getName()} {
	m_selectAfterRedo = iDuplicateRoot.getUUID();
}

DuplicateSubtreeCommand::~DuplicateSubtreeCommand() = default;

void DuplicateSubtreeCommand::undo(scene::Scene& ioScene) {
	if (auto entity = ioScene.findEntityByUUID(m_duplicateSnapshot.entities[0].uuid); entity)
		ioScene.destroyEntityWithChildren(entity);
}

void DuplicateSubtreeCommand::redo(scene::Scene& ioScene) { m_duplicateSnapshot.restore(ioScene); }

auto DuplicateSubtreeCommand::description() const -> std::string {
	return std::format("Duplicate subtree '{}'", m_name);
}

}// namespace owl::nest::commands
