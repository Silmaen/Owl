/**
 * @file ComponentCommands.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ComponentCommands.h"

namespace owl::nest::commands {

namespace {
/**
 * @brief
 *  Restore an entity from a snapshot by destroying the existing one and recreating.
 */
void restoreEntity(scene::Scene& ioScene, const EntitySnapshot& iSnapshot) {
	if (auto existing = ioScene.findEntityByUUID(iSnapshot.uuid); existing)
		ioScene.destroyEntity(existing);
	iSnapshot.restore(ioScene);
}

}// namespace

// --- AddComponentCommand ---
AddComponentCommand::AddComponentCommand(EntitySnapshot iBefore, EntitySnapshot iAfter, std::string iComponentName)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_componentName{std::move(iComponentName)} {
	m_selectAfterUndo = m_after.uuid;
	m_selectAfterRedo = m_after.uuid;
}

AddComponentCommand::~AddComponentCommand() = default;

void AddComponentCommand::undo(scene::Scene& ioScene) {
	if (m_before.uuid != core::UUID{0})
		restoreEntity(ioScene, m_before);
}

void AddComponentCommand::redo(scene::Scene& ioScene) { restoreEntity(ioScene, m_after); }

auto AddComponentCommand::description() const -> std::string {
	return std::format("Add {}", m_componentName);
}

// --- RemoveComponentCommand ---
RemoveComponentCommand::RemoveComponentCommand(EntitySnapshot iBefore, EntitySnapshot iAfter,
											   std::string iComponentName)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_componentName{std::move(iComponentName)} {
	m_selectAfterUndo = m_before.uuid;
	m_selectAfterRedo = m_before.uuid;
}

RemoveComponentCommand::~RemoveComponentCommand() = default;

void RemoveComponentCommand::undo(scene::Scene& ioScene) { restoreEntity(ioScene, m_before); }

void RemoveComponentCommand::redo(scene::Scene& ioScene) { restoreEntity(ioScene, m_after); }

auto RemoveComponentCommand::description() const -> std::string {
	return std::format("Remove {}", m_componentName);
}

// --- ModifyEntityCommand ---
ModifyEntityCommand::ModifyEntityCommand(core::UUID iEntityUuid, EntitySnapshot iBefore, std::string iDescription)
	: m_entityUuid{iEntityUuid}, m_before{std::move(iBefore)}, m_description{std::move(iDescription)} {
	m_selectAfterUndo = m_entityUuid;
	m_selectAfterRedo = m_entityUuid;
}

ModifyEntityCommand::~ModifyEntityCommand() = default;

void ModifyEntityCommand::captureAfter(const scene::Entity& iEntity) {
	m_after = EntitySnapshot::capture(iEntity);
}

void ModifyEntityCommand::undo(scene::Scene& ioScene) { restoreEntity(ioScene, m_before); }

void ModifyEntityCommand::redo(scene::Scene& ioScene) { restoreEntity(ioScene, m_after); }

auto ModifyEntityCommand::description() const -> std::string { return m_description; }

auto ModifyEntityCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const ModifyEntityCommand*>(&iOther);
	if (other == nullptr)
		return false;
	if (other->m_entityUuid != m_entityUuid)
		return false;
	// Keep our "before" state, take the other's "after" state.
	m_after = other->m_after;
	m_timestamp = other->m_timestamp;
	return true;
}

auto ModifyEntityCommand::typeId() const -> size_t {
	// Hash combining the command type and entity UUID for merge matching.
	constexpr size_t typeHash = 0x4D6F644500000000ULL;// "ModE"
	return typeHash ^ std::hash<uint64_t>{}(static_cast<uint64_t>(m_entityUuid));
}

}// namespace owl::nest::commands
