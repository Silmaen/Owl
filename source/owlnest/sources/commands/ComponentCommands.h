/**
 * @file ComponentCommands.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../EntitySnapshot.h"
#include "../UndoCommand.h"

namespace owl::nest::commands {
/**
 * @brief
 *  Command for adding a component to an entity.
 *
 * Redo re-adds the component; undo removes it.
 * Captures the entity state after the add so redo can restore the exact component data.
 */
class AddComponentCommand final : public SceneUndoCommand {
public:
	AddComponentCommand(const AddComponentCommand&) = delete;

	AddComponentCommand(AddComponentCommand&&) = default;

	auto operator=(const AddComponentCommand&) -> AddComponentCommand& = delete;

	auto operator=(AddComponentCommand&&) -> AddComponentCommand& = default;

	/**
	 * @brief
	 *  Construct after a component has been added.
	 * @param[in] iBefore Entity state before the component was added.
	 * @param[in] iAfter Entity state after the component was added.
	 * @param[in] iComponentName Human-readable component name.
	 */
	AddComponentCommand(EntitySnapshot iBefore, EntitySnapshot iAfter, std::string iComponentName);

	/**
	 * @brief
	 *  Destructor.
	 */
	~AddComponentCommand() override;

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
	/// Entity state before the component was added.
	EntitySnapshot m_before;
	/// Entity state after the component was added.
	EntitySnapshot m_after;
	/// Component display name.
	std::string m_componentName;
};
/**
 * @brief
 *  Command for removing a component from an entity.
 *
 * Redo re-removes the component; undo restores it.
 */
class RemoveComponentCommand final : public SceneUndoCommand {
public:
	RemoveComponentCommand(const RemoveComponentCommand&) = delete;

	RemoveComponentCommand(RemoveComponentCommand&&) = default;

	auto operator=(const RemoveComponentCommand&) -> RemoveComponentCommand& = delete;

	auto operator=(RemoveComponentCommand&&) -> RemoveComponentCommand& = default;

	/**
	 * @brief
	 *  Construct before a component is removed.
	 * @param[in] iBefore Entity state before removal (has the component).
	 * @param[in] iAfter Entity state after removal (without the component).
	 * @param[in] iComponentName Human-readable component name.
	 */
	RemoveComponentCommand(EntitySnapshot iBefore, EntitySnapshot iAfter, std::string iComponentName);

	/**
	 * @brief
	 *  Destructor.
	 */
	~RemoveComponentCommand() override;

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
	/// Entity state before removal (has the component).
	EntitySnapshot m_before;
	/// Entity state after removal (without the component).
	EntitySnapshot m_after;
	/// Component display name.
	std::string m_componentName;
};
/**
 * @brief
 *  Command for modifying an entity's properties (any component change).
 *
 * Captures full entity state before/after. Supports merging rapid consecutive
 * edits on the same entity via typeId() + mergeWith().
 */
class ModifyEntityCommand final : public SceneUndoCommand {
public:
	ModifyEntityCommand(const ModifyEntityCommand&) = delete;

	ModifyEntityCommand(ModifyEntityCommand&&) = default;

	auto operator=(const ModifyEntityCommand&) -> ModifyEntityCommand& = delete;

	auto operator=(ModifyEntityCommand&&) -> ModifyEntityCommand& = default;

	/**
	 * @brief
	 *  Construct with the before state. Call captureAfter() when the edit completes.
	 * @param[in] iEntityUuid The entity UUID.
	 * @param[in] iBefore The entity state before modification.
	 * @param[in] iDescription Short description of what changed.
	 */
	ModifyEntityCommand(core::UUID iEntityUuid, EntitySnapshot iBefore, std::string iDescription);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ModifyEntityCommand() override;

	/**
	 * @brief
	 *  Capture the entity's current state as the "after" state.
	 */
	void captureAfter(const scene::Entity& iEntity);

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

	/**
	 * @brief
	 * 	Coalesce two consecutive same-target moves into one undo step.
	 * @param[in] iOther The previously pushed command, candidate for merging.
	 * @return True when the merge succeeded; false otherwise.
	 */
	[[nodiscard]] auto mergeWith(const SceneUndoCommand& iOther) -> bool override;

	/**
	 * @brief
	 * 	Stable identifier of the command type for merge compatibility checks.
	 * @return The command type id.
	 */
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	/// UUID of the entity being modified (looked up at undo/redo time).
	core::UUID m_entityUuid;
	/// Entity state captured before the modification (used by `undo`).
	EntitySnapshot m_before;
	/// Entity state captured after the modification (used by `redo`).
	EntitySnapshot m_after;
	/// Human-readable label shown in the Edit menu undo/redo entries.
	std::string m_description;
};

}// namespace owl::nest::commands
