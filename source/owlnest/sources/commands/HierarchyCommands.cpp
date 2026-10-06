/**
 * @file HierarchyCommands.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HierarchyCommands.h"

#include <scene/component/Transform.h>

#include <format>

namespace owl::nest::commands {
// --- ReparentCommand ---

ReparentCommand::ReparentCommand(const scene::Entity& iChild, const core::UUID iNewParentUuid,
								 const scene::Scene& iScene)
	: m_childUuid{iChild.getUUID()}, m_oldSlot{HierarchySlot::capture(iChild, iScene)}, m_newParentUuid{iNewParentUuid},
	  m_oldLocalTransform{iChild.getComponent<scene::component::Transform>().transform}, m_name{iChild.getName()} {
	m_selectAfterUndo = m_childUuid;
	m_selectAfterRedo = m_childUuid;
}

ReparentCommand::~ReparentCommand() = default;

void ReparentCommand::undo(scene::Scene& ioScene) {
	const auto child = ioScene.findEntityByUUID(m_childUuid);
	if (!child)
		return;
	m_oldSlot.restore(child, ioScene);
	child.getComponent<scene::component::Transform>().transform = m_oldLocalTransform;
}

void ReparentCommand::redo(scene::Scene& ioScene) {
	auto child = ioScene.findEntityByUUID(m_childUuid);
	auto newParent = ioScene.findEntityByUUID(m_newParentUuid);
	if (child && newParent)
		ioScene.setParent(child, newParent);
}

auto ReparentCommand::description() const -> std::string { return std::format("Reparent '{}'", m_name); }

// --- UnparentCommand ---
UnparentCommand::UnparentCommand(const scene::Entity& iChild, const scene::Scene& iScene)
	: m_childUuid{iChild.getUUID()}, m_oldSlot{HierarchySlot::capture(iChild, iScene)},
	  m_oldLocalTransform{iChild.getComponent<scene::component::Transform>().transform}, m_name{iChild.getName()} {
	m_selectAfterUndo = m_childUuid;
	m_selectAfterRedo = m_childUuid;
}

UnparentCommand::~UnparentCommand() = default;

void UnparentCommand::undo(scene::Scene& ioScene) {
	const auto child = ioScene.findEntityByUUID(m_childUuid);
	if (!child)
		return;
	m_oldSlot.restore(child, ioScene);
	child.getComponent<scene::component::Transform>().transform = m_oldLocalTransform;
}

void UnparentCommand::redo(scene::Scene& ioScene) {
	if (auto child = ioScene.findEntityByUUID(m_childUuid); child)
		ioScene.unparent(child);
}

auto UnparentCommand::description() const -> std::string { return std::format("Unparent '{}'", m_name); }

}// namespace owl::nest::commands
