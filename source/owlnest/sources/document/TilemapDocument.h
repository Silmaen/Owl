/**
 * @file TilemapDocument.h
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"

#include "../panel/TilePalette.h"

#include <scene/TilemapAsset.h>

struct ImDrawList;
struct ImVec2;

namespace owl::nest {
/**
 * @brief
 *  Undo manager typed for `scene::TilemapAsset` edits.
 *
 * Each command captures the full asset YAML before/after the edit (coarse-grained
 * but trivially correct). The dedicated `ModifyTilemapAssetCommand` is the only
 * command kind currently routed through this manager.
 */
using TilemapUndoManager = UndoManager<scene::TilemapAsset>;

/**
 * @brief
 *  Document editing a reusable `.owltilemap` tilemap asset.
 *
 * Owns one `scene::TilemapAsset` and exposes a three-pane authoring UI:
 * - **Properties** (left): tileset drop slot, grid size, cell size, layer manager
 *   (add / delete / reorder / visibility / parallax).
 * - **Canvas** (centre): zoomable / pannable top-down grid. Left-click paints the
 *   selected tile from the palette into the active layer's cell; right-click
 *   erases. Each completed stroke pushes one `ModifyTilemapAssetCommand`.
 * - **Palette** (right): the existing `TilePalette` panel, fed with the asset's
 *   resolved tileset.
 *
 * Saves the asset in the engine's `.owltilemap` YAML format. The undo stack is
 * typed over the asset itself so future graph / inline editors can share commands.
 */
class TilemapDocument final : public Document {
public:
	TilemapDocument(const TilemapDocument&) = delete;

	TilemapDocument(TilemapDocument&&) = delete;

	auto operator=(const TilemapDocument&) -> TilemapDocument& = delete;

	auto operator=(TilemapDocument&&) -> TilemapDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	TilemapDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~TilemapDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Document kind.
	 * @return Always `DocumentType::Tilemap`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Tilemap; }

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
	 * @return Always true — the hierarchy panel hosts the tilemap's general properties (tileset
	 *         slot, grid size, layer manager) and the properties panel hosts the per-tile metadata
	 *         editor.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Whether the typed tilemap undo manager has at least one undoable command.
	 * @return True when the editor-wide Undo can be applied to this document.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undoManager.canUndo(); }

	/**
	 * @brief
	 *  Whether the typed tilemap undo manager has at least one redoable command.
	 * @return True when the editor-wide Redo can be applied to this document.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undoManager.canRedo(); }

	/**
	 * @brief
	 *  Dispatch undo against the live `TilemapAsset`.
	 */
	void performUndo() override {
		if (m_undoManager.canUndo())
			m_undoManager.undo(m_asset);
	}

	/**
	 * @brief
	 *  Dispatch redo against the live `TilemapAsset`.
	 */
	void performRedo() override {
		if (m_undoManager.canRedo())
			m_undoManager.redo(m_asset);
	}

	/**
	 * @brief
	 *  Render the hierarchy override (general tilemap properties + layer manager).
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Render the properties override (per-cell info for the inspected canvas cell).
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title displayed for the hierarchy panel while this document is active.
	 * @return The display string ("Tilemap").
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Tilemap"; }

	/**
	 * @brief
	 *  Title displayed for the properties panel while this document is active.
	 * @return The display string ("Cell Properties").
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Cell Properties"; }

	// --- Tilemap-specific API ------------------------------------------------
	/**
	 * @brief
	 *  Typed undo manager for `TilemapAsset` operations.
	 * @return The tilemap undo manager.
	 */
	[[nodiscard]] auto tilemapUndoManager() -> TilemapUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  Const access to the typed undo manager for `TilemapAsset` operations.
	 * @return The tilemap undo manager.
	 */
	[[nodiscard]] auto tilemapUndoManager() const -> const TilemapUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  Direct access to the edited asset.
	 * @return The asset.
	 */
	[[nodiscard]] auto asset() -> scene::TilemapAsset& { return m_asset; }

	/**
	 * @brief
	 *  Const access to the edited asset.
	 * @return The asset.
	 */
	[[nodiscard]] auto asset() const -> const scene::TilemapAsset& { return m_asset; }

	/**
	 * @brief
	 *  Load an asset from disk. Returns false on I/O or parse error.
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
	 *  Render the canvas (zoom/pan grid, click-to-paint) — the entire document tab.
	 */
	void renderCanvas();

	/**
	 * @brief
	 *  Accept a Content-Browser drop on the invisible button covering the canvas — defers
	 *  the open to the next frame to avoid mid-render document-stack mutation.
	 */
	void handleCanvasDrop();

	/**
	 * @brief
	 *  Apply mouse-wheel zoom and middle-mouse pan to `m_zoomPixelsPerCell` / `m_panOffset`.
	 * @param[in] iCanvasOrigin Canvas top-left in screen-space.
	 * @param[in] iHovered Whether the canvas invisible-button is hovered this frame.
	 */
	void handleCanvasZoomPan(const ImVec2& iCanvasOrigin, bool iHovered);

	/**
	 * @brief
	 *  Draw every visible layer's tiles into the canvas via `ImDrawList::AddImageQuad`.
	 * @param[in] iDrawList Active ImGui draw list.
	 * @param[in] iGridTopLeft Top-left of the tilemap grid in screen-space (after pan/zoom).
	 * @param[in] iCellPx Cell size in pixels at the current zoom level.
	 */
	void drawCanvasTiles(ImDrawList* iDrawList, const ImVec2& iGridTopLeft, float iCellPx) const;

	/**
	 * @brief
	 *  Process hover, cell selection on click, and paint stroke on left/right mouse hold.
	 * @param[in] iDrawList Active ImGui draw list (used to draw the hover indicator).
	 * @param[in] iGridTopLeft Grid top-left in screen-space.
	 * @param[in] iGridSize Grid size in pixels (`width * cellPx`, `height * cellPx`).
	 * @param[in] iCellPx Cell size in pixels.
	 */
	void handleCanvasHoverAndPaint(ImDrawList* iDrawList, const ImVec2& iGridTopLeft, const ImVec2& iGridSize,
								   float iCellPx);

	/**
	 * @brief
	 *  Refresh the saved-snapshot baseline used by `isDirty()`.
	 */
	void refreshSavedSnapshot();

	/**
	 * @brief
	 *  Resolve the asset's tileset from `tilesetPath` if not yet loaded.
	 *
	 * Mirrors `Scene::resolveAllTilemapAssets` for the document's standalone asset.
	 * Idempotent.
	 */
	void resolveTileset();

	/**
	 * @brief
	 *  Begin a paint stroke: capture the asset's pre-edit YAML for undo.
	 */
	void beginStroke();

	/**
	 * @brief
	 *  End a paint stroke: push a `ModifyTilemapAssetCommand` if anything changed.
	 */
	void endStroke();

	/// Disk path for the asset backing this document.
	std::filesystem::path m_path;
	/// Asset currently being edited.
	scene::TilemapAsset m_asset;
	/// Per-document undo/redo manager for tilemap edits.
	TilemapUndoManager m_undoManager;
	/// YAML snapshot taken at the last save — drives the dirty indicator.
	std::string m_savedSnapshot;
	/// Editor layer back-pointer for menus/dialogs that need editor-wide context.
	EditorLayer* mp_editorLayer{nullptr};
	/// Tile palette state (selected layer + brush).
	panel::TilePalette m_palette;

	/// Canvas zoom in pixels per cell (clamped at the call site).
	float m_zoomPixelsPerCell = 32.f;
	/// Canvas pan offset in pixels.
	math::vec2 m_panOffset{0.f, 0.f};
	/// Whether a paint stroke is currently active (between mouse-down and mouse-up).
	bool m_strokeInProgress = false;
	/// Asset YAML captured at the start of the active stroke (used for undo).
	std::string m_strokeBeforeYaml;
	/// Currently inspected cell on the canvas (X on the active painting layer). Negative = none.
	int32_t m_inspectedCellX = -1;
	/// Currently inspected cell on the canvas (Y on the active painting layer). Negative = none.
	int32_t m_inspectedCellY = -1;

	/// Tracks the ImGui "Open" state of the document tab.
	bool m_pOpen{true};
	/// Whether the document window was focused last frame (used to detect focus transitions).
	bool m_wasFocused{false};

	/// Placeholder unused — `Document::undoManager()` requires returning a `SceneUndoManager&`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
