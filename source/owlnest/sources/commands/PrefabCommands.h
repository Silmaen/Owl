/**
 * @file PrefabCommands.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../EntitySnapshot.h"
#include "../UndoCommand.h"

#include <string>
#include <vector>

namespace owl::nest::commands {
/**
 * @brief
 *  Override marks of a prefab instance before and after an edit, so that undoing the edit puts them back.
 *
 * The marks live in the `PrefabLink` of the instance root, which is not the edited entity when a child
 * of the instance is edited: the entity snapshots of the edit command do not cover them.
 */
struct PrefabOverrideChange {
	/// Root of the prefab instance holding the edited entity (0 when the entity is not part of an instance).
	core::UUID rootUuid{0};
	/// Override marks before the edit.
	std::vector<std::string> before;
	/// Override marks after the edit.
	std::vector<std::string> after;

	/**
	 * @brief
	 *  Capture the override marks of the instance holding an entity, before an edit.
	 * @param[in] iEntity The entity about to be edited.
	 * @param[in] iScene The scene holding the entity.
	 * @return The change, with `after` equal to `before` until `captureAfter` is called.
	 */
	[[nodiscard]] static auto capture(const scene::Entity& iEntity, const scene::Scene& iScene) -> PrefabOverrideChange;

	/**
	 * @brief
	 *  Record the overrides an edit made (`PrefabSerializer::recordOverrides`) and capture the change.
	 * @param[in] iEntity The edited entity.
	 * @param[in] iScene The scene holding the entity.
	 * @param[in] iBeforeYaml The entity YAML before the edit.
	 * @return The change (empty when the entity is not part of an instance or nothing new is overridden).
	 */
	[[nodiscard]] static auto record(const scene::Entity& iEntity, const scene::Scene& iScene,
									 const std::string& iBeforeYaml) -> PrefabOverrideChange;

	/**
	 * @brief
	 *  Capture the override marks after the edit.
	 * @param[in] iScene The scene holding the instance.
	 */
	void captureAfter(const scene::Scene& iScene);

	/**
	 * @brief
	 *  Check whether the edit changed nothing in the override marks.
	 * @return True when there is nothing to restore.
	 */
	[[nodiscard]] auto isEmpty() const -> bool;

	/**
	 * @brief
	 *  Put the marks captured before the edit back on the instance.
	 * @param[in,out] ioScene The scene holding the instance.
	 */
	void restoreBefore(scene::Scene& ioScene) const;

	/**
	 * @brief
	 *  Put the marks captured after the edit back on the instance.
	 * @param[in,out] ioScene The scene holding the instance.
	 */
	void restoreAfter(scene::Scene& ioScene) const;

	/**
	 * @brief
	 *  Fold a later change of the same edit sequence into this one (merged undo steps).
	 * @param[in] iLater The later change.
	 */
	void mergeWith(const PrefabOverrideChange& iLater);
};

/**
 * @brief
 *  Command for instantiating a prefab into the scene.
 *
 * Redo restores the instantiated subtree; undo destroys it.
 */
class InstantiatePrefabCommand final : public SceneUndoCommand {
public:
	InstantiatePrefabCommand(const InstantiatePrefabCommand&) = delete;

	InstantiatePrefabCommand(InstantiatePrefabCommand&&) = default;

	auto operator=(const InstantiatePrefabCommand&) -> InstantiatePrefabCommand& = delete;

	auto operator=(InstantiatePrefabCommand&&) -> InstantiatePrefabCommand& = default;

	/**
	 * @brief
	 *  Construct after a prefab has been instantiated.
	 * @param[in] iInstanceRoot The root entity of the instantiated subtree.
	 * @param[in] iScene The scene (for capturing the subtree snapshot).
	 * @param[in] iPrefabName Human-readable prefab name.
	 */
	InstantiatePrefabCommand(const scene::Entity& iInstanceRoot, const scene::Scene& iScene, std::string iPrefabName);

	/**
	 * @brief
	 *  Destructor.
	 */
	~InstantiatePrefabCommand() override;

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
	/// Snapshot of the instantiated subtree.
	SubtreeSnapshot m_snapshot;
	/// Prefab display name.
	std::string m_prefabName;
};
/**
 * @brief
 *  Command for applying prefab updates or reverting an instance.
 *
 * Captures the full instance subtree before and after the operation. Undo and redo update the entities
 * in place, destroy the ones the other state does not have (entities added or removed by the prefab)
 * and recreate the missing ones with their UUIDs.
 */
class ApplyPrefabCommand final : public SceneUndoCommand {
public:
	ApplyPrefabCommand(const ApplyPrefabCommand&) = delete;

	ApplyPrefabCommand(ApplyPrefabCommand&&) = default;

	auto operator=(const ApplyPrefabCommand&) -> ApplyPrefabCommand& = delete;

	auto operator=(ApplyPrefabCommand&&) -> ApplyPrefabCommand& = default;

	/**
	 * @brief
	 *  Construct with before/after subtree snapshots.
	 * @param[in] iBefore Subtree state before the operation.
	 * @param[in] iAfter Subtree state after the operation.
	 * @param[in] iDescription Human-readable description.
	 */
	ApplyPrefabCommand(SubtreeSnapshot iBefore, SubtreeSnapshot iAfter, std::string iDescription);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ApplyPrefabCommand() override;

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
	/// Instance subtree state before the operation.
	SubtreeSnapshot m_before;
	/// Instance subtree state after the operation.
	SubtreeSnapshot m_after;
	/// Description.
	std::string m_description;
};

}// namespace owl::nest::commands
