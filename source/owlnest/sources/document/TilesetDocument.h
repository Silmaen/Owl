/**
 * @file TilesetDocument.h
 * @author Silmaen
 * @date 09/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"

#include <scene/Tileset.h>

namespace owl::nest {
/**
 * @brief
 *  Undo manager typed for `scene::Tileset` edits.
 *
 * Each command captures the full tileset YAML before / after the edit (coarse-grained
 * but trivially correct, mirroring `TilemapUndoManager`).
 */
using TilesetUndoManager = UndoManager<scene::Tileset>;

/**
 * @brief
 *  Document editing a reusable `.owltileset` atlas asset.
 *
 * Three-pane authoring UI delivered through the global panels override:
 * - **Scene Hierarchy panel** hosts the atlas-level properties (texture drop
 *   slot, grid columns / rows, per-tile pixel size, sampler filter).
 * - **Properties panel** hosts the per-tile metadata editor (collidable
 *   flag, designer-friendly name) for the currently inspected slot.
 * - The document tab itself shows the atlas preview canvas: the texture
 *   tiled with a grid overlay, click-to-select each slot, the selected
 *   slot highlighted in the editor accent colour.
 *
 * Saves the tileset in the engine's `.owltileset` YAML format. The undo
 * stack is typed over the tileset itself so future graph editors can share
 * commands.
 */
class TilesetDocument final : public Document {
public:
	TilesetDocument(const TilesetDocument&) = delete;

	TilesetDocument(TilesetDocument&&) = delete;

	auto operator=(const TilesetDocument&) -> TilesetDocument& = delete;

	auto operator=(TilesetDocument&&) -> TilesetDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	TilesetDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~TilesetDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Document kind.
	 * @return Always `DocumentType::Tileset`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Tileset; }

	/**
	 * @brief
	 *  Tab title displayed in the document tab bar (file stem, or "Untitled").
	 * @return The display title.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  On-disk path of the document.
	 * @return The document file path (empty if never saved).
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the document has unsaved changes.
	 * @return True when the document is dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool override;

	/**
	 * @brief
	 *  Called once when the document is attached to the manager.
	 * @param[in] iEditor Owning editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Called once when the document is being removed from the manager.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update tick.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Dispatch an input/system event to the document.
	 * @param[in,out] ioEvent The incoming event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Render the document's panels in the current ImGui frame.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Save the document to its current file path.
	 * @return True on success; false on failure or when no path is set.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save the document to an explicit path.
	 * @param[in] iPath Destination file.
	 * @return True on success; false on failure.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Access to the document's undo/redo stack (placeholder for the `Document` base — not used).
	 * @return The empty placeholder undo manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Const access to the document's undo/redo stack (placeholder).
	 * @return The empty placeholder undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Indicate that the document repurposes the global Scene Hierarchy & Properties panels.
	 * @return Always true.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Whether the typed tileset undo manager has at least one undoable command.
	 * @return True when the editor-wide Undo can be applied to this document.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undoManager.canUndo(); }

	/**
	 * @brief
	 *  Whether the typed tileset undo manager has at least one redoable command.
	 * @return True when the editor-wide Redo can be applied to this document.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undoManager.canRedo(); }

	/**
	 * @brief
	 *  Dispatch undo against the live `Tileset`.
	 */
	void performUndo() override {
		if (m_undoManager.canUndo())
			m_undoManager.undo(m_tileset);
	}

	/**
	 * @brief
	 *  Dispatch redo against the live `Tileset`.
	 */
	void performRedo() override {
		if (m_undoManager.canRedo())
			m_undoManager.redo(m_tileset);
	}

	/**
	 * @brief
	 *  Render the hierarchy override (texture, grid, filter mode).
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Render the properties override (per-tile metadata for the selected slot).
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title displayed for the hierarchy panel while this document is active.
	 * @return The display string ("Tileset").
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Tileset"; }

	/**
	 * @brief
	 *  Title displayed for the properties panel while this document is active.
	 * @return The display string ("Tile Properties").
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Tile Properties"; }

	// --- Tileset-specific API ------------------------------------------------
	/**
	 * @brief
	 *  Typed undo manager for `Tileset` operations.
	 * @return The tileset undo manager.
	 */
	[[nodiscard]] auto tilesetUndoManager() -> TilesetUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  Const access to the typed undo manager.
	 * @return The tileset undo manager.
	 */
	[[nodiscard]] auto tilesetUndoManager() const -> const TilesetUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  Direct access to the edited tileset.
	 * @return The tileset.
	 */
	[[nodiscard]] auto tileset() -> scene::Tileset& { return m_tileset; }

	/**
	 * @brief
	 *  Const access to the edited tileset.
	 * @return The tileset.
	 */
	[[nodiscard]] auto tileset() const -> const scene::Tileset& { return m_tileset; }

	/**
	 * @brief
	 *  Load a tileset from disk. Returns false on I/O or parse error.
	 * @param[in] iPath The source file.
	 * @return True on success.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  True while the user hasn't clicked the tab's close X.
	 * @return True until ImGui flips the close flag.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Reset the open flag (used by `EditorLayer` after a close is handled or cancelled).
	 * @param[in] iOpen The new value.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Render the atlas preview canvas (full texture + grid overlay + click-to-select).
	 */
	void renderCanvas();

	/**
	 * @brief
	 *  Refresh the saved-snapshot baseline used by `isDirty()`.
	 */
	void refreshSavedSnapshot();

	/**
	 * @brief
	 *  Resolve the texture from its `getSerializeString()` after a load.
	 *
	 * The Tileset YAML stores a serialized texture name; on load the asset's
	 * `texture` ptr is already populated by `Tileset::deserializeFromString`
	 * via `Texture2D::createFromSerializedForDeserialize`, so this is mostly
	 * a no-op kept for symmetry with `TilemapDocument::resolveTileset`.
	 */
	void resolveTexture();

	/// Disk path for the asset backing this document.
	std::filesystem::path m_path;
	/// Tileset currently being edited.
	scene::Tileset m_tileset;
	/// Per-document undo/redo manager for tileset edits.
	TilesetUndoManager m_undoManager;
	/// YAML snapshot taken at the last save — drives the dirty indicator.
	std::string m_savedSnapshot;
	/// Editor layer back-pointer for menus/dialogs that need editor-wide context.
	EditorLayer* mp_editorLayer{nullptr};
	/// Currently inspected tile (the slot whose metadata is shown in the Properties panel). `-1` = none.
	int32_t m_inspectedTile = -1;

	/// Canvas zoom factor — 1.0 = atlas rendered at native pixel size.
	float m_zoom = 1.f;
	/// Canvas pan offset in pixels.
	math::vec2 m_panOffset{0.f, 0.f};

	/// Tracks the ImGui "Open" state of the document tab.
	bool m_pOpen{true};
	/// Whether the document window was focused last frame (used to detect focus transitions).
	bool m_wasFocused{false};

	/// Placeholder unused — `Document::undoManager()` requires returning a `SceneUndoManager&`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
