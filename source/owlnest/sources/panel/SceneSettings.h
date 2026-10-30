/**
 * @file SceneSettings.h
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owlgui.h>

#include "../Project.h"
#include "../UndoManager.h"
#include "../commands/CommandRegistry.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace YAML {
class Node;
}// namespace YAML

namespace owl::nest::panel {

/**
 * @brief
 *  Dockable panel for editing the active scene's global settings.
 *
 * Surfaces the per-scene `EnabledRenderers` block (renderer-stack ordering,
 * enable / disable per layer, per-layer-type override editor) without
 * requiring the user to hand-edit the `.owl` file. Engine semantics are owned
 * by `renderer::EnabledRenderersConfig`; this panel is pure UI plumbing on
 * top of it.
 *
 * The panel is **dockable** (lives alongside `SceneHierarchy`,
 * `ContentBrowser`, etc.). It pushes `commands::ModifyEnabledRenderersCommand`
 * onto the active scene's undo manager whenever the config changes — within
 * the manager's 1s merge window successive edits collapse into a single
 * history entry, matching the behaviour of the inspector's slider edits.
 *
 * After every successful edit the caller is expected to refresh the active
 * `RenderStack` (the `EditorLayer::syncActiveDocumentPanels` path already
 * does this). The panel exposes `consumeDirtyFlag` so the editor can detect
 * when a rebuild is required without polling the scene every frame.
 */
class SceneSettings final {
public:
	SceneSettings(const SceneSettings&) = delete;
	SceneSettings(SceneSettings&&) = delete;
	auto operator=(const SceneSettings&) -> SceneSettings& = delete;
	auto operator=(SceneSettings&&) -> SceneSettings& = delete;

	SceneSettings() = default;
	~SceneSettings() = default;

	/**
	 * @brief
	 *  Make the window visible. Toggleable from the editor menu.
	 */
	void open() { m_visible = true; }

	/**
	 * @brief
	 *  Set the project's renderer-stack config (the catalogue of layer
	 * names and types the panel offers).
	 * @param[in] iProject The active project (read-only — only the
	 * `rendererStack` field is consumed).
	 */
	void setProject(const Project& iProject) { mp_project = &iProject; }

	/**
	 * @brief
	 *  Set the active scene this panel edits.
	 * @param[in] iScene The active scene (may be null when no document is
	 * open — the panel will render an empty placeholder in that case).
	 */
	void setScene(const shared<scene::Scene>& iScene) { m_scene = iScene; }

	/**
	 * @brief
	 *  Set the command registry the HUD choice is issued through.
	 * @param[in] iCommands The registry (non-owning, may be null: the choice is then refused).
	 */
	void setCommandRegistry(const commands::CommandRegistry* iCommands) { mp_commands = iCommands; }

	/**
	 * @brief
	 *  Set the undo manager to push commands onto.
	 * @param[in] iUndoManager Owning manager pointer (may be null when no
	 * document is active).
	 */
	void setUndoManager(SceneUndoManager* iUndoManager) { mp_undoManager = iUndoManager; }

	/**
	 * @brief
	 *  Render the panel. Call once per frame.
	 */
	void onImGuiRender();

	/**
	 * @brief
	 *  Whether the user changed the config since the last call to
	 * `consumeDirtyFlag`. Used by the editor to decide when to rebuild the
	 * active `RenderStack` from the scene + project config.
	 * @return True if a config change occurred since the last consume.
	 */
	[[nodiscard]] auto consumeDirtyFlag() -> bool {
		const bool wasDirty = m_dirty;
		m_dirty = false;
		return wasDirty;
	}

private:
	/**
	 * @brief
	 *  Render the renderer-stack section.
	 * @return true if the config changed.
	 */
	auto renderRendererStackSection() -> bool;
	/**
	 * @brief
	 *  Render the per-layer override widgets dispatching on layer type.
	 * @return true if any field changed.
	 */
	static auto renderLayerOverridesEditor(const std::string& iTypeKey, YAML::Node& ioOverrides) -> bool;
	/**
	 * @brief
	 *  Render the "Add layer slot" ghost rows for project layers absent
	 *  from the scene listing. Returns true if a layer was added.
	 * @return True on success, false otherwise.
	 */
	auto renderAddLayerSection() -> bool;

	/**
	 * @brief
	 *  Render the physics section (sub-steps, solver threads, interpolation); pushes an
	 *  undoable `ModifyPhysicsSettingsCommand` on change.
	 */
	void renderPhysicsSection() const;

	/**
	 * @brief
	 *  Render the world origin section (floating origin, shift radius, current origin); runs
	 *  `scene.set_floating_origin` on change.
	 */
	void renderWorldOriginSection() const;

	/**
	 * @brief
	 *  Render the world partition section: streaming radii, the table of cells (select many, add, open, change the
	 *  scene, move or remove the selected ones), each edit a `scene.*world*` command.
	 */
	void renderWorldPartitionSection();

	/**
	 * @brief
	 *  Run a command on the active scene through the panel's command target.
	 * @param[in] iName The command.
	 * @param[in] iArgs Its arguments.
	 * @return True when it ran.
	 */
	[[nodiscard]] auto runCommand(const std::string& iName, const commands::CommandArgs& iArgs) const -> bool;

	/**
	 * @brief
	 *  Render the HUD section: the `.owlhud` the scene draws (project default, none or a file), chosen through the
	 *  undoable `scene.set_hud` command.
	 */
	void renderHudSection();

	/**
	 * @brief
	 *  Render the Script section: the script the scene owns, with its hooks and properties, edited through the
	 *  undoable `scene.set_script` command.
	 */
	void renderScriptSection();

	/// Active project (non-owning).
	const Project* mp_project = nullptr;
	/// Command registry (non-owning).
	const commands::CommandRegistry* mp_commands = nullptr;
	/// `.owlhud` files offered by the HUD choice, listed when it opens.
	std::vector<std::string> m_hudFiles;
	/// Whether the HUD choice was open on the last frame (the list is read again when it opens).
	bool m_hudChoiceOpen = false;
	/// Copy of the scene script the inspector widgets edit (kept between frames: they remember it by address).
	scene::component::LuaScript m_scriptEdit;
	/// Cells selected in the world partition table.
	std::vector<math::vec3i> m_selectedCells;
	/// Coordinates typed for the next cell.
	std::array<int32_t, 3> m_newCell{0, 0, 0};
	/// Offset typed to move the selected cells.
	std::array<int32_t, 3> m_cellOffset{1, 0, 0};
	/// Active scene (shared_ptr — same lifetime as `SceneDocument`).
	shared<scene::Scene> m_scene;
	/// Undo manager for the active scene (non-owning).
	SceneUndoManager* mp_undoManager = nullptr;
	/// Whether the panel window is visible.
	bool m_visible = false;
	/// YAML snapshot taken at the start of the frame, used to detect changes.
	std::string m_frameSnapshot;
	/// Set whenever a config change is committed; consumed by the editor.
	bool m_dirty = false;
};

}// namespace owl::nest::panel
