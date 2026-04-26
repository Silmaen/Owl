/**
 * @file HierarchyCommands.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoCommand.h"

#include <math/Transform.h>

namespace owl::nest::commands {

/**
 * @brief Command for reparenting an entity (drag-drop in hierarchy or context menu).
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
	 * @brief Construct before setParent is called.
	 * @param[in] iChild The child entity being reparented.
	 * @param[in] iNewParentUuid UUID of the new parent entity.
	 */
	ReparentCommand(const scene::Entity& iChild, core::UUID iNewParentUuid);
	~ReparentCommand() override;

	void undo(scene::Scene& ioScene) override;
	void redo(scene::Scene& ioScene) override;
	[[nodiscard]] auto description() const -> std::string override;

private:
	core::UUID m_childUuid;
	core::UUID m_oldParentUuid;
	core::UUID m_newParentUuid;
	math::Transform m_oldLocalTransform;
	std::string m_name;
};

/**
 * @brief Command for unparenting an entity (making it a root entity).
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
	 * @brief Construct before unparent is called.
	 * @param[in] iChild The child entity being unparented.
	 */
	explicit UnparentCommand(const scene::Entity& iChild);
	~UnparentCommand() override;

	void undo(scene::Scene& ioScene) override;
	void redo(scene::Scene& ioScene) override;
	[[nodiscard]] auto description() const -> std::string override;

private:
	core::UUID m_childUuid;
	core::UUID m_oldParentUuid;
	math::Transform m_oldLocalTransform;
	std::string m_name;
};

}// namespace owl::nest::commands
