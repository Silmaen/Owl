/**
 * @file HierarchyCommands.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HierarchyCommands.h"

#include <scene/Scene.h>
#include <scene/component/MeshSockets.h>
#include <scene/component/Transform.h>

#include <format>
#include <string>
#include <tuple>
#include <utility>

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

// --- SocketCommand ---
SocketCommand::SocketCommand(const scene::Entity& iEntity, const core::UUID iOwnerUuid, std::string iSocket,
							 const scene::Scene& iScene)
	: m_entityUuid{iEntity.getUUID()}, m_ownerUuid{iOwnerUuid}, m_socket{std::move(iSocket)},
	  m_oldSlot{HierarchySlot::capture(iEntity, iScene)},
	  m_oldLocalTransform{iEntity.getComponent<scene::component::Transform>().transform},
	  m_wasAttached{iEntity.hasComponent<scene::component::SocketAttachment>()}, m_name{iEntity.getName()} {
	if (m_wasAttached)
		m_oldSocket = iEntity.getComponent<scene::component::SocketAttachment>().socket;
	m_selectAfterUndo = m_entityUuid;
	m_selectAfterRedo = m_entityUuid;
}

SocketCommand::~SocketCommand() = default;

void SocketCommand::undo(scene::Scene& ioScene) {
	auto entity = ioScene.findEntityByUUID(m_entityUuid);
	if (!entity)
		return;
	if (entity.hasComponent<scene::component::SocketAttachment>())
		entity.removeComponent<scene::component::SocketAttachment>();
	m_oldSlot.restore(entity, ioScene);
	entity.getComponent<scene::component::Transform>().transform = m_oldLocalTransform;
	if (m_wasAttached)
		entity.addComponent<scene::component::SocketAttachment>().socket = m_oldSocket;
}

void SocketCommand::redo(scene::Scene& ioScene) {
	const auto entity = ioScene.findEntityByUUID(m_entityUuid);
	if (!entity)
		return;
	if (m_ownerUuid == core::UUID{0}) {
		std::ignore = ioScene.detachFromSocket(entity);
		return;
	}
	std::ignore = ioScene.attachToSocket(entity, ioScene.findEntityByUUID(m_ownerUuid), m_socket);
}

auto SocketCommand::description() const -> std::string {
	return m_ownerUuid == core::UUID{0} ? std::format("Release '{}' from its socket", m_name)
										: std::format("Attach '{}' to socket {}", m_name, m_socket);
}

}// namespace owl::nest::commands
