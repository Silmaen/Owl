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
#include <scene/TilesetAtlas.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

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
 * The atlas image is composed in the document: an image dropped on a slot fills it (scaled to the cell), a folder
 * fills the slot and the next empty ones (rows added at the bottom when the grid is full), a cleared slot keeps its
 * index, selected slots move together by drag. The pixels live in a `scene::TilesetAtlas`, shown at once through the
 * shared atlas texture and written to the atlas PNG on save.
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
	 *  Current content, serialized in the document's file format.
	 * @return The content.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Replace the content with an autosaved snapshot, keeping the path; the document becomes dirty.
	 * @param[in] iSnapshot The serialized content.
	 * @return True when the snapshot was applied.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

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

	// --- Atlas composition ---------------------------------------------------
	/**
	 * @brief
	 *  Fill one slot with an image file, scaled to the cell (one undo entry).
	 * @param[in] iIndex The slot.
	 * @param[in] iImage The image file (`.png`, `.jpg`).
	 * @return True when the slot changed.
	 */
	auto setTileImage(uint32_t iIndex, const std::filesystem::path& iImage) -> bool;

	/**
	 * @brief
	 *  Fill a slot and the next empty ones with images, rows added at the bottom when the grid is full (one undo
	 *  entry).
	 * @param[in] iFrom The slot the first image goes to.
	 * @param[in] iImages The image files, in order; a folder stands for its images sorted by name.
	 * @return True when the atlas changed.
	 */
	auto insertTileImages(uint32_t iFrom, const std::vector<std::filesystem::path>& iImages) -> bool;

	/**
	 * @brief
	 *  Make slots transparent and reset their metadata, their indices kept (one undo entry).
	 * @param[in] iTiles The slots.
	 * @return True when the atlas changed.
	 */
	auto clearTiles(const std::vector<uint32_t>& iTiles) -> bool;

	/**
	 * @brief
	 *  Move slots by an index offset, pixels and metadata, each exchanged with the slot it lands on (one undo
	 *  entry).
	 * @param[in] iTiles The slots.
	 * @param[in] iOffset The index offset.
	 * @return False when a slot would leave the grid (nothing moves).
	 */
	auto moveTiles(const std::vector<uint32_t>& iTiles, int32_t iOffset) -> bool;

	/**
	 * @brief
	 *  Change the grid, the pixels and the metadata of each slot kept at its column and row (one undo entry).
	 * @param[in] iColumns New column count.
	 * @param[in] iRows New row count.
	 */
	void resizeGrid(uint32_t iColumns, uint32_t iRows);

	/**
	 * @brief
	 *  Select slots in the atlas canvas.
	 * @param[in] iIndex The slot clicked.
	 * @param[in] iToggle Add it to the selection or remove it (Ctrl).
	 * @param[in] iRange Select from the inspected slot to it (Shift).
	 */
	void selectTile(uint32_t iIndex, bool iToggle = false, bool iRange = false);

	/**
	 * @brief
	 *  Selected slots, ascending.
	 * @return The slots.
	 */
	[[nodiscard]] auto getSelectedTiles() const -> const std::vector<uint32_t>& { return m_selectedTiles; }

	/**
	 * @brief
	 *  Pixels of the atlas, read from its image at the first composition.
	 * @return The atlas, none before the first composition.
	 */
	[[nodiscard]] auto getAtlas() const -> const std::optional<scene::TilesetAtlas>& { return m_atlas; }

	/**
	 * @brief
	 *  Image file the atlas is written to on save.
	 * @return The file, empty before the first composition.
	 */
	[[nodiscard]] auto getAtlasFile() const -> const std::filesystem::path& { return m_atlasFile; }

	/**
	 * @brief
	 *  Put the atlas back to a grid and cell pixels (undo and redo of a composition).
	 * @param[in] iColumns Column count.
	 * @param[in] iRows Row count.
	 * @param[in] iCells Cells to write: column, row, pixels.
	 */
	void restoreAtlas(uint32_t iColumns, uint32_t iRows,
					  const std::vector<std::tuple<uint32_t, uint32_t, std::vector<uint8_t>>>& iCells);

private:
	/**
	 * @brief
	 *  Read the atlas pixels from the atlas image, or start a transparent one next to the tileset.
	 * @return True when the atlas can be composed.
	 */
	auto ensureAtlas() -> bool;

	/**
	 * @brief
	 *  Run a composition: metadata and pixel changes recorded as one undo entry, the texture updated.
	 * @param[in] iDescription The undo entry.
	 * @param[in] iMutator The change, returning false when nothing happened.
	 * @return The result of the mutator.
	 */
	auto composeAtlas(const std::string& iDescription, const std::function<bool()>& iMutator) -> bool;

	/**
	 * @brief
	 *  Upload the atlas pixels to the tileset texture (mipmaps built per tile for a pixel-art tileset).
	 */
	void uploadAtlas();

	/**
	 * @brief
	 *  Context menu of the atlas canvas: insert, clear, select.
	 */
	void renderCanvasMenu();

	/**
	 * @brief
	 *  Render the slope table of an isometric tileset: the tile of each terrain id on the 19 slopes.
	 */
	void renderSlopeSection();

	/**
	 * @brief
	 *  Navigation section of the Tile Properties: the road connections of the inspected tile, one arrow per side.
	 */
	void renderConnectionsSection();

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
	/// Selected slots, ascending (the inspected slot among them).
	std::vector<uint32_t> m_selectedTiles;
	/// Slot a selection drag started on.
	std::optional<uint32_t> m_dragFrom;
	/// Pixels of the atlas being composed.
	std::optional<scene::TilesetAtlas> m_atlas;
	/// Image file the atlas is written to.
	std::filesystem::path m_atlasFile;
	/// Hash of the atlas pixels, and the one at the last save: the atlas half of `isDirty()`.
	uint64_t m_atlasHash{0};
	/// Hash of the atlas pixels written by the last save.
	uint64_t m_savedAtlasHash{0};

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
