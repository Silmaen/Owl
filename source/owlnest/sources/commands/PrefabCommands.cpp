/**
 * @file PrefabCommands.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "PrefabCommands.h"

#include <format>
#include <utility>

namespace owl::nest::commands {
// --- InstantiatePrefabCommand ---

InstantiatePrefabCommand::InstantiatePrefabCommand(const scene::Entity& iInstanceRoot, const scene::Scene& iScene,
												   std::string iPrefabName)
	: m_snapshot{SubtreeSnapshot::capture(iInstanceRoot, iScene)}, m_prefabName{std::move(iPrefabName)} {
	m_selectAfterRedo = iInstanceRoot.getUUID();
}

InstantiatePrefabCommand::~InstantiatePrefabCommand() = default;

void InstantiatePrefabCommand::undo(scene::Scene& ioScene) {
	if (m_snapshot.entities.empty())
		return;
	if (auto root = ioScene.findEntityByUUID(m_snapshot.entities[0].uuid); root)
		ioScene.destroyEntityWithChildren(root);
}

void InstantiatePrefabCommand::redo(scene::Scene& ioScene) { m_snapshot.restore(ioScene); }

auto InstantiatePrefabCommand::description() const -> std::string {
	return std::format("Instantiate '{}'", m_prefabName);
}

// --- ApplyPrefabCommand ---
ApplyPrefabCommand::ApplyPrefabCommand(SubtreeSnapshot iBefore, SubtreeSnapshot iAfter, std::string iDescription)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)} {
	if (!m_before.entities.empty())
		m_selectAfterUndo = m_before.entities[0].uuid;
	if (!m_after.entities.empty())
		m_selectAfterRedo = m_after.entities[0].uuid;
}

ApplyPrefabCommand::~ApplyPrefabCommand() = default;

void ApplyPrefabCommand::undo(scene::Scene& ioScene) { m_before.restore(ioScene); }

void ApplyPrefabCommand::redo(scene::Scene& ioScene) { m_after.restore(ioScene); }

auto ApplyPrefabCommand::description() const -> std::string { return m_description; }

}// namespace owl::nest::commands
