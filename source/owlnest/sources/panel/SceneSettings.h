/**
 * @file SceneSettings.h
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include "../Project.h"
#include "../UndoManager.h"

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

	/// Active project (non-owning).
	const Project* mp_project = nullptr;
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
