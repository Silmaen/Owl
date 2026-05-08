/**
 * @file HierarchyCommands.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HierarchyCommands.h"

#include <scene/component/Hierarchy.h>
#include <scene/component/Transform.h>

namespace owl::nest::commands {
// --- ReparentCommand ---

ReparentCommand::ReparentCommand(const scene::Entity& iChild, const core::UUID iNewParentUuid)
	: m_childUuid{iChild.getUUID()}, m_oldParentUuid{iChild.getComponent<scene::component::Hierarchy>().parentId},
	  m_newParentUuid{iNewParentUuid},
	  m_oldLocalTransform{iChild.getComponent<scene::component::Transform>().transform}, m_name{iChild.getName()} {
	m_selectAfterUndo = m_childUuid;
	m_selectAfterRedo = m_childUuid;
}

ReparentCommand::~ReparentCommand() = default;

void ReparentCommand::undo(scene::Scene& ioScene) {
	auto child = ioScene.findEntityByUUID(m_childUuid);
	if (!child)
		return;
	// Restore original parent.
	if (m_oldParentUuid != core::UUID{0}) {
		if (auto oldParent = ioScene.findEntityByUUID(m_oldParentUuid); oldParent)
			ioScene.setParent(child, oldParent);
	} else {
		ioScene.unparent(child);
	}
	// Restore old local transform (setParent/unparent recomputes it, so overwrite).
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
UnparentCommand::UnparentCommand(const scene::Entity& iChild)
	: m_childUuid{iChild.getUUID()}, m_oldParentUuid{iChild.getComponent<scene::component::Hierarchy>().parentId},
	  m_oldLocalTransform{iChild.getComponent<scene::component::Transform>().transform}, m_name{iChild.getName()} {
	m_selectAfterUndo = m_childUuid;
	m_selectAfterRedo = m_childUuid;
}

UnparentCommand::~UnparentCommand() = default;

void UnparentCommand::undo(scene::Scene& ioScene) {
	auto child = ioScene.findEntityByUUID(m_childUuid);
	if (!child)
		return;
	if (auto oldParent = ioScene.findEntityByUUID(m_oldParentUuid); oldParent)
		ioScene.setParent(child, oldParent);
	// Restore old local transform.
	child.getComponent<scene::component::Transform>().transform = m_oldLocalTransform;
}

void UnparentCommand::redo(scene::Scene& ioScene) {
	if (auto child = ioScene.findEntityByUUID(m_childUuid); child)
		ioScene.unparent(child);
}

auto UnparentCommand::description() const -> std::string { return std::format("Unparent '{}'", m_name); }

}// namespace owl::nest::commands
