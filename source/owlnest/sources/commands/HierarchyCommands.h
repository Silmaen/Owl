/**
 * @file HierarchyCommands.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../EntitySnapshot.h"
#include "../UndoCommand.h"

#include <math/Transform.h>

#include <string>

namespace owl::nest::commands {
/**
 * @brief
 *  Command for reparenting an entity (drag-drop in hierarchy or context menu).
 *
 * Redo calls setParent(child, newParent). Undo restores the old parent and local transform.
 */
class ReparentCommand final : public SceneUndoCommand {
public:
	ReparentCommand(const ReparentCommand&) = delete;

	ReparentCommand(ReparentCommand&&) = default;

	auto operator=(const ReparentCommand&) -> ReparentCommand& = delete;

	auto operator=(ReparentCommand&&) -> ReparentCommand& = default;

	/**
	 * @brief
	 *  Construct before setParent is called.
	 * @param[in] iChild The child entity being reparented.
	 * @param[in] iNewParentUuid UUID of the new parent entity.
	 * @param[in] iScene The scene holding the entity.
	 */
	ReparentCommand(const scene::Entity& iChild, core::UUID iNewParentUuid, const scene::Scene& iScene);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ReparentCommand() override;

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// UUID of the entity being reparented.
	core::UUID m_childUuid;
	/// Parent and sibling index before the reparent (restored on undo).
	HierarchySlot m_oldSlot;
	/// Parent UUID after the reparent (used by redo).
	core::UUID m_newParentUuid;
	/// Child's local transform before reparenting (preserved on undo).
	math::Transform m_oldLocalTransform;
	/// Display name used in the Edit-menu undo/redo entry.
	std::string m_name;
};
/**
 * @brief
 *  Command for unparenting an entity (making it a root entity).
 *
 * Redo calls unparent(child). Undo restores the old parent and local transform.
 */
class UnparentCommand final : public SceneUndoCommand {
public:
	UnparentCommand(const UnparentCommand&) = delete;

	UnparentCommand(UnparentCommand&&) = default;

	auto operator=(const UnparentCommand&) -> UnparentCommand& = delete;

	auto operator=(UnparentCommand&&) -> UnparentCommand& = default;

	/**
	 * @brief
	 *  Construct before unparent is called.
	 * @param[in] iChild The child entity being unparented.
	 * @param[in] iScene The scene holding the entity.
	 */
	UnparentCommand(const scene::Entity& iChild, const scene::Scene& iScene);

	/**
	 * @brief
	 *  Destructor.
	 */
	~UnparentCommand() override;

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioScene The target scene the action is applied to.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// UUID of the entity being unparented.
	core::UUID m_childUuid;
	/// Parent and sibling index before the unparent (restored on undo).
	HierarchySlot m_oldSlot;
	/// Child's local transform before unparenting (preserved on undo).
	math::Transform m_oldLocalTransform;
	/// Display name used in the Edit-menu undo/redo entry.
	std::string m_name;
};

/**
 * @brief
 *  Command snapping an entity on a socket of an owner (`Scene::attachToSocket`), or releasing it from its socket.
 *
 * Undo puts the entity back under its former parent, with its former transform and its former socket.
 */
class SocketCommand final : public SceneUndoCommand {
public:
	SocketCommand(const SocketCommand&) = delete;

	SocketCommand(SocketCommand&&) = default;

	auto operator=(const SocketCommand&) -> SocketCommand& = delete;

	auto operator=(SocketCommand&&) -> SocketCommand& = default;

	/**
	 * @brief
	 *  Capture the state of the entity before the change.
	 * @param[in] iEntity The entity attached or released.
	 * @param[in] iOwnerUuid Owner of the socket; 0 to release the entity.
	 * @param[in] iSocket Name of the socket (ignored when releasing).
	 * @param[in] iScene The scene of the entity.
	 */
	SocketCommand(const scene::Entity& iEntity, core::UUID iOwnerUuid, std::string iSocket, const scene::Scene& iScene);

	/**
	 * @brief
	 *  Destructor.
	 */
	~SocketCommand() override;

	/**
	 * @brief
	 *  Restore the parent, the transform and the socket of the entity.
	 * @param[in,out] ioScene The scene.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Attach or release the entity.
	 * @param[in,out] ioScene The scene.
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Get the description of the command.
	 * @return The description.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// UUID of the entity.
	core::UUID m_entityUuid;
	/// Owner of the socket, 0 to release.
	core::UUID m_ownerUuid;
	/// Name of the socket.
	std::string m_socket;
	/// Parent and sibling index before the change.
	HierarchySlot m_oldSlot;
	/// Local transform before the change.
	math::Transform m_oldLocalTransform;
	/// Socket the entity was attached to before the change, empty for none.
	std::string m_oldSocket;
	/// Whether the entity was attached to a socket before the change.
	bool m_wasAttached{false};
	/// Display name used in the Edit-menu undo/redo entry.
	std::string m_name;
};

}// namespace owl::nest::commands
