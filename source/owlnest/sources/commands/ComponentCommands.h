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
 * @brief Command for adding a component to an entity.
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
	 * @brief Construct after a component has been added.
	 * @param[in] iBefore Entity state before the component was added.
	 * @param[in] iAfter Entity state after the component was added.
	 * @param[in] iComponentName Human-readable component name.
	 */
	AddComponentCommand(EntitySnapshot iBefore, EntitySnapshot iAfter, std::string iComponentName);
	~AddComponentCommand() override;

	void undo(scene::Scene& ioScene) override;
	void redo(scene::Scene& ioScene) override;
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
 * @brief Command for removing a component from an entity.
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
	 * @brief Construct before a component is removed.
	 * @param[in] iBefore Entity state before removal (has the component).
	 * @param[in] iAfter Entity state after removal (without the component).
	 * @param[in] iComponentName Human-readable component name.
	 */
	RemoveComponentCommand(EntitySnapshot iBefore, EntitySnapshot iAfter, std::string iComponentName);
	~RemoveComponentCommand() override;

	void undo(scene::Scene& ioScene) override;
	void redo(scene::Scene& ioScene) override;
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
 * @brief Command for modifying an entity's properties (any component change).
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
	 * @brief Construct with the before state. Call captureAfter() when the edit completes.
	 * @param[in] iEntityUuid The entity UUID.
	 * @param[in] iBefore The entity state before modification.
	 * @param[in] iDescription Short description of what changed.
	 */
	ModifyEntityCommand(core::UUID iEntityUuid, EntitySnapshot iBefore, std::string iDescription);
	~ModifyEntityCommand() override;

	/// Capture the entity's current state as the "after" state.
	void captureAfter(const scene::Entity& iEntity);

	void undo(scene::Scene& ioScene) override;
	void redo(scene::Scene& ioScene) override;
	[[nodiscard]] auto description() const -> std::string override;
	[[nodiscard]] auto mergeWith(const SceneUndoCommand& iOther) -> bool override;
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	core::UUID m_entityUuid;
	EntitySnapshot m_before;
	EntitySnapshot m_after;
	std::string m_description;
};

}// namespace owl::nest::commands
