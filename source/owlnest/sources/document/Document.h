/**
 * @file Document.h
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include "../UndoManager.h"

namespace owl::nest {

class EditorLayer;

/**
 * @brief
 *  Kind of editable document opened in a tab.
 */
enum struct DocumentType : uint8_t {
	Scene,///< A `.owl` scene edited in the Viewport.
	Code,///< A text/source file edited with syntax highlighting.
	NodeGraph,///< A `.owlflow` node-graph document (Scene Flow, future animation graphs, behaviour trees...).
	Animation,///< A `.owlanim` reusable spritesheet animation clip.
	Tilemap,///< A `.owltilemap` reusable tilemap level (grid + layers + tileset reference).
	Tileset,///< A `.owltileset` atlas asset (texture + grid + per-tile metadata).
};

/**
 * @brief
 *  Abstract base class for an editable document (scene, script, graph...).
 *
 * Each `Document` owns the state bound to one editable artefact: its file path,
 * its dirty flag, its undo/redo stack, and any per-document panels
 * (e.g. a `Viewport` for a scene). The editor holds a `DocumentManager` that
 * lists the open documents and tracks which one is currently active. Global
 * panels (scene hierarchy, inspector) show data from the active document.
 */
class Document {
public:
	Document(const Document&) = delete;

	Document(Document&&) = delete;

	auto operator=(const Document&) -> Document& = delete;

	auto operator=(Document&&) -> Document& = delete;

	Document() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~Document();

	/**
	 * @brief
	 *  Unique id for this document (stable across its lifetime).
	 * @return The core UUID.
	 */
	[[nodiscard]] auto id() const -> core::UUID { return m_id; }

	/**
	 * @brief
	 *  Document kind (scene, lua script...).
	 * @return The DocumentType = 0.
	 */
	[[nodiscard]] virtual auto type() const -> DocumentType = 0;

	/**
	 * @brief
	 *  Displayed title on the tab (without dirty marker).
	 * @return The std string = 0.
	 */
	[[nodiscard]] virtual auto title() const -> std::string = 0;

	/**
	 * @brief
	 *  On-disk path, empty if untitled/never saved.
	 * @return The std filesystem path = 0.
	 */
	[[nodiscard]] virtual auto filePath() const -> std::filesystem::path = 0;

	/**
	 * @brief
	 *  True when the document has unsaved changes.
	 * @return The bool = 0.
	 */
	[[nodiscard]] virtual auto isDirty() const -> bool = 0;

	/**
	 * @brief
	 *  Called once when the document is inserted into the manager.
	 */
	virtual void onAttach(EditorLayer* iEditor) = 0;

	/**
	 * @brief
	 *  Called once when the document is being removed.
	 */
	virtual void onDetach() = 0;

	/**
	 * @brief
	 *  Per-frame update tick (scene logic, runtime update...).
	 */
	virtual void onUpdate(const core::Timestep& iTimeStep) = 0;

	/**
	 * @brief
	 *  Event dispatch (keyboard, mouse, file drop).
	 */
	virtual void onEvent(event::Event& ioEvent) = 0;

	/**
	 * @brief
	 *  Per-frame ImGui rendering of the document's own panels (e.g. viewport).
	 */
	virtual void onImGuiRender() = 0;

	/**
	 * @brief
	 *  Save to the current file path. Returns false on failure or if no path is set.
	 * @return The bool = 0.
	 */
	virtual auto save() -> bool = 0;

	/**
	 * @brief
	 *  Save to an explicit path. Returns false on failure.
	 * @return The bool = 0.
	 */
	virtual auto saveAs(const std::filesystem::path& iPath) -> bool = 0;

	/**
	 * @brief
	 *  Access to the document's undo/redo stack.
	 * @return The SceneUndoManager = 0.
	 */
	[[nodiscard]] virtual auto undoManager() -> SceneUndoManager& = 0;

	/**
	 * @brief
	 *  Const access to the document's undo/redo stack.
	 * @return The document's undo manager.
	 */
	[[nodiscard]] virtual auto undoManager() const -> const SceneUndoManager& = 0;

	/**
	 * @brief
	 *  When true, the editor's global Scene Hierarchy + Properties panels delegate rendering
	 *        to `renderHierarchyPanel()` / `renderPropertiesPanel()` while this document is active.
	 * @return False by default — only documents that do not edit a `scene::Scene` (e.g. node-graph
	 *         views) typically override this.
	 */
	[[nodiscard]] virtual auto overridesGlobalPanels() const -> bool { return false; }

	/**
	 * @brief
	 *  Whether the document's typed undo stack has at least one undoable command.
	 * @return False unless overridden — base documents have no document-level undo.
	 */
	[[nodiscard]] virtual auto canUndo() const -> bool { return false; }

	/**
	 * @brief
	 *  Whether the document's typed undo stack has at least one redoable command.
	 * @return False unless overridden.
	 */
	[[nodiscard]] virtual auto canRedo() const -> bool { return false; }

	/**
	 * @brief
	 *  Apply the document's typed undo command (no-op when `canUndo()` returns false).
	 *
	 * The default implementation is empty — concrete documents (`SceneDocument`,
	 * `TilemapDocument`, `TilesetDocument`, …) override this to dispatch to their typed
	 * `UndoManager`.  The editor-wide Edit ribbon's Undo/Redo buttons and the `edit.undo` /
	 * `edit.redo` keyboard shortcuts call this method on the active document.
	 */
	virtual void performUndo() {}

	/**
	 * @brief
	 *  Apply the document's typed redo command (no-op when `canRedo()` returns false).
	 */
	virtual void performRedo() {}

	/**
	 * @brief
	 *  Custom content for the global "Scene Hierarchy" panel (called between `ImGui::Begin`/`End`).
	 */
	virtual void renderHierarchyPanel() {}

	/**
	 * @brief
	 *  Custom content for the global "Properties" panel (called between `ImGui::Begin`/`End`).
	 */
	virtual void renderPropertiesPanel() {}

	/**
	 * @brief
	 *  Title displayed in the title bar of the global "Scene Hierarchy" panel while this
	 *  document is active. The panel's ImGui ID stays stable (kept under `###Hierarchy`) so
	 *  the dock layout is preserved across the rename.
	 * @return The display title (e.g. "Scene Hierarchy", "Tilemap", "Tileset"). The
	 *         default implementation returns the scene-only label.
	 */
	[[nodiscard]] virtual auto hierarchyPanelTitle() const -> std::string { return "Scene Hierarchy"; }

	/**
	 * @brief
	 *  Title displayed in the title bar of the global "Properties" panel while this document
	 *  is active. Same stable-ID trick (`###Properties`) as `hierarchyPanelTitle`.
	 * @return The display title.
	 */
	[[nodiscard]] virtual auto propertiesPanelTitle() const -> std::string { return "Properties"; }

	/**
	 * @brief
	 *  Mark the document as needing keyboard / tab focus on its next ImGui render.
	 *
	 * Concrete documents call `ImGui::SetNextWindowFocus()` before their `ImGui::Begin` when
	 * `consumeFocusRequest()` returns true. The flag defaults to true on construction so a
	 * freshly-opened document automatically becomes the visible tab in its dock.
	 */
	void requestFocus() { m_focusRequested = true; }

	/**
	 * @brief
	 *  Read-and-clear the focus request flag. Called by concrete documents inside
	 *  `onImGuiRender` immediately before the document's main `ImGui::Begin`.
	 * @return True when the document should focus its window this frame.
	 */
	[[nodiscard]] auto consumeFocusRequest() -> bool {
		const bool req = m_focusRequested;
		m_focusRequested = false;
		return req;
	}

private:
	/// Stable identifier used by the UI (tab id, ImGui window suffix).
	core::UUID m_id;
	/// True when the next render should pull keyboard / tab focus to this document's window.
	bool m_focusRequested = true;
};

}// namespace owl::nest
