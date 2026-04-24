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

namespace owl::nest::commands {

/**
 * @brief Command for instantiating a prefab into the scene.
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
	 * @brief Construct after a prefab has been instantiated.
	 * @param[in] iInstanceRoot The root entity of the instantiated subtree.
	 * @param[in] iScene The scene (for capturing the subtree snapshot).
	 * @param[in] iPrefabName Human-readable prefab name.
	 */
	InstantiatePrefabCommand(const scene::Entity& iInstanceRoot, const scene::Scene& iScene,
							 std::string iPrefabName);
	~InstantiatePrefabCommand() override;

	void undo(scene::Scene& ioScene) override;
	void redo(scene::Scene& ioScene) override;
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// Snapshot of the instantiated subtree.
	SubtreeSnapshot m_snapshot;
	/// Prefab display name.
	std::string m_prefabName;
};

/**
 * @brief Command for applying prefab updates or reverting an instance.
 *
 * Captures the full instance subtree before and after the operation.
 */
class ApplyPrefabCommand final : public SceneUndoCommand {
public:
	ApplyPrefabCommand(const ApplyPrefabCommand&) = delete;
	ApplyPrefabCommand(ApplyPrefabCommand&&) = default;
	auto operator=(const ApplyPrefabCommand&) -> ApplyPrefabCommand& = delete;
	auto operator=(ApplyPrefabCommand&&) -> ApplyPrefabCommand& = default;
	/**
	 * @brief Construct with before/after subtree snapshots.
	 * @param[in] iBefore Subtree state before the operation.
	 * @param[in] iAfter Subtree state after the operation.
	 * @param[in] iDescription Human-readable description.
	 */
	ApplyPrefabCommand(SubtreeSnapshot iBefore, SubtreeSnapshot iAfter, std::string iDescription);
	~ApplyPrefabCommand() override;

	void undo(scene::Scene& ioScene) override;
	void redo(scene::Scene& ioScene) override;
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
