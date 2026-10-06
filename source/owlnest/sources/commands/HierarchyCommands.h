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

}// namespace owl::nest::commands
