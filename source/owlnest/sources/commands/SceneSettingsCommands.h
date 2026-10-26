/**
 * @file SceneSettingsCommands.h
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoCommand.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <cstddef>
#include <optional>
#include <string>

namespace owl::nest::commands {

/**
 * @brief
 *  Undoable edit of a scene's `EnabledRenderers` block (per-scene
 * renderer-stack overrides).
 *
 * Snapshots the entire `EnabledRenderersConfig` as YAML strings (before / after)
 * because the config is small (≤ a handful of layers) and the round-trip is
 * exact. `undo` and `redo` re-parse the matching snapshot via
 * `EnabledRenderersConfig::fromYaml` and assign it onto the scene.
 *
 * Supports merging: when the user is rapidly tweaking a slider in the override
 * editor, consecutive commands within the `UndoManager` 1s window collapse into
 * a single history entry — the per-scene-settings analogue of the
 * `ModifyEntityCommand` coalescing used by the inspector.
 */
class ModifyEnabledRenderersCommand final : public SceneUndoCommand {
public:
	ModifyEnabledRenderersCommand(const ModifyEnabledRenderersCommand&) = delete;
	ModifyEnabledRenderersCommand(ModifyEnabledRenderersCommand&&) = default;
	auto operator=(const ModifyEnabledRenderersCommand&) -> ModifyEnabledRenderersCommand& = delete;
	auto operator=(ModifyEnabledRenderersCommand&&) -> ModifyEnabledRenderersCommand& = default;

	/**
	 * @brief
	 *  Construct from before/after YAML snapshots.
	 * @param[in] iBeforeYaml YAML serialisation of the config prior to the edit.
	 * @param[in] iAfterYaml YAML serialisation of the config after the edit.
	 */
	ModifyEnabledRenderersCommand(std::string iBeforeYaml, std::string iAfterYaml);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ModifyEnabledRenderersCommand() override;

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

	/**
	 * @brief
	 *  Read-only access to the "before" snapshot (used by tests).
	 * @return The const std string.
	 */
	[[nodiscard]] auto beforeYaml() const -> const std::string& { return m_beforeYaml; }
	/**
	 * @brief
	 *  Read-only access to the "after" snapshot (used by tests).
	 * @return The const std string.
	 */
	[[nodiscard]] auto afterYaml() const -> const std::string& { return m_afterYaml; }

private:
	/**
	 * @brief
	 *  Apply a YAML snapshot onto the scene's `EnabledRenderersConfig`.
	 */
	static void apply(scene::Scene& ioScene, const std::string& iYaml);

	/// YAML snapshot of `EnabledRenderersConfig` before the edit.
	std::string m_beforeYaml;
	/// YAML snapshot of `EnabledRenderersConfig` after the edit.
	std::string m_afterYaml;
};

/**
 * @brief
 *  Undoable edit of a scene's `PhysicsSettings` (sub-steps, solver threads, interpolation).
 *
 * Holds the settings before and after the edit; successive edits within the `UndoManager` merge
 * window collapse into one history entry.
 */
class ModifyPhysicsSettingsCommand final : public SceneUndoCommand {
public:
	ModifyPhysicsSettingsCommand(const ModifyPhysicsSettingsCommand&) = delete;

	ModifyPhysicsSettingsCommand(ModifyPhysicsSettingsCommand&&) = default;

	auto operator=(const ModifyPhysicsSettingsCommand&) -> ModifyPhysicsSettingsCommand& = delete;

	auto operator=(ModifyPhysicsSettingsCommand&&) -> ModifyPhysicsSettingsCommand& = default;

	/**
	 * @brief
	 *  Construct from the settings before and after the edit.
	 * @param[in] iBefore Settings prior to the edit.
	 * @param[in] iAfter Settings after the edit.
	 */
	ModifyPhysicsSettingsCommand(const scene::PhysicsSettings& iBefore, const scene::PhysicsSettings& iAfter);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ModifyPhysicsSettingsCommand() override;

	/**
	 * @brief
	 *  Restore the settings prior to the edit.
	 * @param[in,out] ioScene The target scene.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Apply the edited settings again.
	 * @param[in,out] ioScene The target scene.
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
	 *  Coalesce two consecutive physics-settings edits into one undo step.
	 * @param[in] iOther The previously pushed command, candidate for merging.
	 * @return True when the merge succeeded; false otherwise.
	 */
	[[nodiscard]] auto mergeWith(const SceneUndoCommand& iOther) -> bool override;

	/**
	 * @brief
	 *  Stable identifier of the command type for merge compatibility checks.
	 * @return The command type id.
	 */
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	/// Settings before the edit.
	scene::PhysicsSettings m_before;
	/// Settings after the edit.
	scene::PhysicsSettings m_after;
};

/**
 * @brief
 *  Undoable edit of a scene's `FloatingOriginSettings` (enabled, shift radius).
 *
 * Successive edits within the `UndoManager` merge window collapse into one history entry.
 */
class SetFloatingOriginCommand final : public SceneUndoCommand {
public:
	SetFloatingOriginCommand(const SetFloatingOriginCommand&) = delete;

	SetFloatingOriginCommand(SetFloatingOriginCommand&&) = default;

	auto operator=(const SetFloatingOriginCommand&) -> SetFloatingOriginCommand& = delete;

	auto operator=(SetFloatingOriginCommand&&) -> SetFloatingOriginCommand& = default;

	/**
	 * @brief
	 *  Construct from the settings before and after the edit.
	 * @param[in] iBefore Settings prior to the edit.
	 * @param[in] iAfter Settings after the edit.
	 */
	SetFloatingOriginCommand(const scene::FloatingOriginSettings& iBefore, const scene::FloatingOriginSettings& iAfter);

	/**
	 * @brief
	 *  Destructor.
	 */
	~SetFloatingOriginCommand() override;

	/**
	 * @brief
	 *  Restore the settings prior to the edit.
	 * @param[in,out] ioScene The target scene.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Apply the edited settings again.
	 * @param[in,out] ioScene The target scene.
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
	 *  Coalesce two consecutive floating origin edits into one undo step.
	 * @param[in] iOther The newer command.
	 * @return True when the merge succeeded.
	 */
	[[nodiscard]] auto mergeWith(const SceneUndoCommand& iOther) -> bool override;

	/**
	 * @brief
	 *  Stable identifier of the command type for merge compatibility checks.
	 * @return The command type id.
	 */
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	/// Settings before the edit.
	scene::FloatingOriginSettings m_before;
	/// Settings after the edit.
	scene::FloatingOriginSettings m_after;
};

/**
 * @brief
 *  Undoable choice of the HUD asset a scene draws (its `Hud:` key).
 */
class SetSceneHudCommand final : public SceneUndoCommand {
public:
	SetSceneHudCommand(const SetSceneHudCommand&) = delete;

	SetSceneHudCommand(SetSceneHudCommand&&) = default;

	auto operator=(const SetSceneHudCommand&) -> SetSceneHudCommand& = delete;

	auto operator=(SetSceneHudCommand&&) -> SetSceneHudCommand& = default;

	/**
	 * @brief
	 *  Construct from the HUD path before and after the change.
	 * @param[in] iBefore Path before (nothing: the project's default HUD, empty: none).
	 * @param[in] iAfter Path after.
	 */
	SetSceneHudCommand(std::optional<std::string> iBefore, std::optional<std::string> iAfter);

	/**
	 * @brief
	 *  Destructor.
	 */
	~SetSceneHudCommand() override;

	/**
	 * @brief
	 *  Restore the previous HUD.
	 * @param[in,out] ioScene The target scene.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Choose the new HUD again.
	 * @param[in,out] ioScene The target scene.
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
	 *  Stable identifier of the command type for merge compatibility checks.
	 * @return The command type id.
	 */
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	/// HUD path before the change.
	std::optional<std::string> m_before;
	/// HUD path after the change.
	std::optional<std::string> m_after;
};

/**
 * @brief
 *  Undoable choice of the script a scene owns (its `SceneScript:` key): the path and the property values.
 *
 * Consecutive edits within the merge window of the undo manager collapse into one step.
 */
class SetSceneScriptCommand final : public SceneUndoCommand {
public:
	SetSceneScriptCommand(const SetSceneScriptCommand&) = delete;

	SetSceneScriptCommand(SetSceneScriptCommand&&) = default;

	auto operator=(const SetSceneScriptCommand&) -> SetSceneScriptCommand& = delete;

	auto operator=(SetSceneScriptCommand&&) -> SetSceneScriptCommand& = default;

	/**
	 * @brief
	 *  Construct from the script before and after the change.
	 * @param[in] iBefore Script before (`component::LuaScript::toYaml` text).
	 * @param[in] iAfter Script after.
	 */
	SetSceneScriptCommand(std::string iBefore, std::string iAfter);

	/**
	 * @brief
	 *  Destructor.
	 */
	~SetSceneScriptCommand() override;

	/**
	 * @brief
	 *  Restore the previous script.
	 * @param[in,out] ioScene The target scene.
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Apply the new script again.
	 * @param[in,out] ioScene The target scene.
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
	 *  Coalesce two consecutive edits of the scene script into one undo step.
	 * @param[in] iOther The newer command.
	 * @return True when the merge succeeded.
	 */
	[[nodiscard]] auto mergeWith(const SceneUndoCommand& iOther) -> bool override;

	/**
	 * @brief
	 *  Stable identifier of the command type for merge compatibility checks.
	 * @return The command type id.
	 */
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	/**
	 * @brief
	 *  Put a script on the scene.
	 * @param[in,out] ioScene The target scene.
	 * @param[in] iYaml The script as `component::LuaScript::toYaml` writes it; empty for none.
	 */
	static void apply(scene::Scene& ioScene, const std::string& iYaml);

	/// Script before the change.
	std::string m_before;
	/// Script after the change.
	std::string m_after;
};

}// namespace owl::nest::commands
