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

}// namespace owl::nest::commands
