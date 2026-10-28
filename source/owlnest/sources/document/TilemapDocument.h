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

#include "../commands/TilemapCommands.h"
#include "../panel/TilePalette.h"

#include <scene/TilemapAsset.h>
#include <scene/TilemapTerrain.h>
#include <scene/pcg/PcgGraph.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace owl::nest {
/**
 * @brief
 *  Undo manager typed for `scene::TilemapAsset` edits.
 *
 * Cell edits push a `commands::ModifyTilemapChunksCommand` holding the chunks they changed; layout edits (tileset,
 * size, layers) a `commands::ModifyTilemapLayoutCommand` whose asset copies share their chunks.
 */
using TilemapUndoManager = UndoManager<scene::TilemapAsset>;

/// Tools of the isometric preview of a tilemap.
enum struct IsometricTool : uint8_t {
	Paint,///< Paint terrain tiles with the palette brushes.
	Raise,///< Left raises a corner (or an area), right lowers it.
	Lower,///< Left lowers a corner (or an area), right raises it.
	Level,///< Bring a corner (or an area) to the level height.
	Overlay,///< Paint the overlay layer (tracks, roads) with the palette brush.
	Node,///< Select corners (nodes) and move them up or down by dragging or with the + / - keys.
};

/**
 * @brief
 *  Document editing a reusable `.owltilemap` tilemap asset.
 *
 * Owns one `scene::TilemapAsset` and exposes a three-pane authoring UI:
 * - **Properties** (left): tileset drop slot, grid size, cell size, layer manager
 *   (add / delete / reorder / visibility / parallax).
 * - **Canvas** (centre): zoomable / pannable top-down grid. Left-click paints the
 *   selected tile from the palette into the active layer's cell; right-click
 *   erases. Each completed stroke pushes one `ModifyTilemapChunksCommand` holding the changed chunks.
 * - **Palette** (right): the existing `TilePalette` panel, fed with the asset's
 *   resolved tileset.
 *
 * Saves the asset in the engine's `.owltilemap` format (YAML header, binary chunks). The undo stack is
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
	 *  Change the procedural generation settings (one undo entry).
	 * @param[in] iGenerator The settings, none to paint by hand only.
	 */
	void setGenerator(const std::optional<scene::TilemapGenerator>& iGenerator);

	/**
	 * @brief
	 *  Write the tiles of the generator graph on its layer and region (one undo entry).
	 * @param[in] iSeed A new seed kept in the settings, none to keep the seed.
	 * @return True when cells changed.
	 */
	auto generate(std::optional<uint64_t> iSeed = std::nullopt) -> bool;

	/**
	 * @brief
	 *  Result of the generator on the tilemap, evaluated again when the settings or the graph file changed.
	 * @return The result, none without generator or when its graph cannot be read or evaluated.
	 */
	auto getGeneratorPreview() -> const std::optional<scene::pcg::Result>&;

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

	// --- Node (corner) height editing ----------------------------------------
	/**
	 * @brief
	 *  Corner of the grid at one corner of a cell.
	 * @param[in] iCellX Cell x.
	 * @param[in] iCellY Cell y.
	 * @param[in] iIndex Corner of the cell: 0 back, 1 right, 2 front, 3 left (as `TilemapAsset::getCellCorners`).
	 * @return The corner coordinates in the corner grid.
	 */
	[[nodiscard]] static auto getCellCornerNode(uint32_t iCellX, uint32_t iCellY, size_t iIndex) -> math::vec2ui;

	/**
	 * @brief
	 *  Nodes (corners) selected for height editing.
	 * @return The selected corners, in selection order.
	 */
	[[nodiscard]] auto getSelectedNodes() const -> const std::vector<math::vec2ui>& { return m_selectedNodes; }

	/**
	 * @brief
	 *  Select a node.
	 * @param[in] iNode Corner coordinates, ignored outside the corner grid.
	 * @param[in] iAdditive True to toggle it in the selection, false to select it alone.
	 */
	void selectNode(const math::vec2ui& iNode, bool iAdditive);

	/**
	 * @brief
	 *  Empty the node selection.
	 */
	void clearNodeSelection() { m_selectedNodes.clear(); }

	/**
	 * @brief
	 *  Set the height of a node without closing the undo step; the neighbours follow as the terraform tools do.
	 *
	 * Successive calls are merged by `commitTerrainEdit` into one command.
	 * @param[in] iX Corner x.
	 * @param[in] iY Corner y.
	 * @param[in] iHeight New height in steps.
	 * @return True when a corner moved.
	 */
	auto previewNodeHeight(uint32_t iX, uint32_t iY, uint8_t iHeight) -> bool;

	/**
	 * @brief
	 *  Close the pending terrain edits (node previews) into one undo step.
	 * @param[in] iDescription Description for the Edit menu.
	 * @return True when a command was pushed.
	 */
	auto commitTerrainEdit(const std::string& iDescription) -> bool;

	/**
	 * @brief
	 *  Set the height of a node as one undo step.
	 * @param[in] iX Corner x.
	 * @param[in] iY Corner y.
	 * @param[in] iHeight New height in steps.
	 * @return True when a corner moved.
	 */
	auto setNodeHeight(uint32_t iX, uint32_t iY, uint8_t iHeight) -> bool;

	/**
	 * @brief
	 *  Raise or lower every selected node by the same number of steps, as one undo step.
	 * @param[in] iDelta Steps (negative to lower); heights stay within `[0, maxCornerHeight]`.
	 * @return True when a corner moved.
	 */
	auto moveSelectedNodes(int iDelta) -> bool;

private:
	/**
	 * @brief
	 *  Start height of a selected node when a drag began.
	 */
	struct NodeDragStart {
		/// Corner coordinates.
		math::vec2ui node{0, 0};
		/// Height when the drag began.
		uint8_t height = 0;
	};

	/**
	 * @brief
	 *  Preview a node drag: put the previous preview back and apply `iDelta` steps to the nodes held at the start.
	 * @param[in] iDelta Steps from the drag start.
	 */
	void previewNodeDrag(int iDelta);

	/**
	 * @brief
	 *  Hover highlight, selection, drag and keys of the Node tool.
	 */
	void handleNodeInput(ImDrawList* iDrawList, const ImVec2& iOrigin);

	/**
	 * @brief
	 *  Corner rows of the cell panel: the four heights, editable, and the derived slope.
	 */
	void renderCellCornerRows(uint32_t iX, uint32_t iY);

	/**
	 * @brief
	 *  Render the canvas (zoom/pan grid, click-to-paint) — the entire document tab.
	 */
	void renderCanvas();

	/**
	 * @brief
	 *  Grid section of the hierarchy panel: the tileset, the size and the cell size.
	 */
	void renderGridSection();

	/**
	 * @brief
	 *  Layers section of the hierarchy panel: one row per layer (paint target, name, visibility, order, delete).
	 */
	void renderLayerSection();

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
	 *  Whether the tileset is isometric, which swaps the top-down canvas for the isometric preview.
	 * @return True for a tileset of `kind: Isometric`.
	 */
	[[nodiscard]] auto isIsometric() const -> bool;

	/**
	 * @brief
	 *  Isometric preview of the map with the terraforming tools.
	 */
	void renderIsometricCanvas();

	/**
	 * @brief
	 *  Row of tool buttons and options above the isometric preview.
	 */
	void renderIsometricToolbar();

	/**
	 * @brief
	 *  Draw the cells, back to front, of the isometric preview.
	 */
	void drawIsometricTiles(ImDrawList* iDrawList, const ImVec2& iOrigin, const ImVec2& iCanvasMin,
							const ImVec2& iCanvasMax) const;

	/**
	 * @brief
	 *  Hover highlight, picking and tools of the isometric preview.
	 */
	void handleIsometricInput(ImDrawList* iDrawList, const ImVec2& iOrigin);

	/**
	 * @brief
	 *  Apply the active tool at a picked corner.
	 */
	void applyIsometricTool(const scene::terrain::TerrainHit& iHit, bool iLeftDown, bool iRightDown);

	/**
	 * @brief
	 *  Close the terraforming and cell strokes of the gesture that ended.
	 */
	void finishIsometricGesture();

	/**
	 * @brief
	 *  Paint the primary or secondary brush into a cell of the active layer.
	 */
	void applyBrush(uint32_t iCellX, uint32_t iCellY, bool iLeftDown, bool iRightDown);

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
	 *  Overlay and flags rows of the cell panel, edited through the current stroke.
	 * @param[in] iLayer Layer index of the inspected cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 */
	void renderCellDataRows(uint32_t iLayer, uint32_t iX, uint32_t iY);

	/**
	 * @brief
	 *  End the current stroke: push the chunks it changed as one `ModifyTilemapChunksCommand`.
	 * @param[in] iDescription Description of the command.
	 */
	void endStroke(const std::string& iDescription);

	/**
	 * @brief
	 *  Procedural section of the hierarchy panel: graph, seed, layer, region, preview and Generate.
	 */
	void renderGeneratorSection();

	/**
	 * @brief
	 *  Read the graph of the generator, its path resolved in the asset directories.
	 * @return The graph, none when it cannot be read.
	 */
	[[nodiscard]] auto loadGeneratorGraph() const -> std::optional<scene::pcg::Graph>;

	/**
	 * @brief
	 *  Draw the preview of the generator over the canvas, with its region outlined.
	 * @param[in] iDrawList Destination draw list.
	 * @param[in] iGridTopLeft Top-left of the grid on screen.
	 * @param[in] iCellPx Size of a cell on screen.
	 */
	void drawGeneratorPreview(ImDrawList* iDrawList, const ImVec2& iGridTopLeft, float iCellPx);

	/// Disk path for the asset backing this document.
	std::filesystem::path m_path;
	/// Asset currently being edited.
	scene::TilemapAsset m_asset;
	/// Per-document undo/redo manager for tilemap edits.
	TilemapUndoManager m_undoManager;
	/// Asset as last saved (its chunks shared with `m_asset` until written) — drives the dirty indicator.
	scene::TilemapAsset m_savedAsset;
	/// Asset at the start of a grid resize drag, which the preview resizes from.
	std::optional<scene::TilemapAsset> m_resizeOrigin;
	/// Cell edits in progress, pushed as one undo step when the gesture ends.
	commands::TilemapStroke m_stroke;
	/// Terraforming in progress, pushed as one undo step when the gesture ends.
	commands::TerrainStroke m_terrainStroke;
	/// Tool of the isometric preview.
	IsometricTool m_isoTool = IsometricTool::Paint;
	/// Side, in corners, of the raise / lower / level area.
	int m_terrainBrushSize = 1;
	/// Height the level tool brings its area to, in corner steps.
	int m_levelHeight = 0;
	/// Nodes selected with the Node tool.
	std::vector<math::vec2ui> m_selectedNodes;
	/// Heights of the selected nodes when the current drag began (empty when no drag).
	std::vector<NodeDragStart> m_nodeDragStart;
	/// Corner changes the drag has applied so far, put back before each new preview.
	std::vector<scene::CornerHeightChange> m_nodeDragChanges;
	/// Mouse y when the drag began.
	float m_nodeDragMouseY = 0.f;
	/// Steps the drag currently applies.
	int m_nodeDragDelta = 0;
	/// Whether a press on the Node tool may still turn into a click on the node (no drag yet).
	bool m_nodeClickPending = false;
	/// Corner the terrain tool acted on last, so a drag acts once per corner.
	std::optional<math::vec2ui> m_lastTerrainCorner;
	/// Corner and cell under the cursor in the isometric preview.
	std::optional<scene::terrain::TerrainHit> m_isoHover;
	/// Whether the preview has been centred once.
	bool m_isoCentred = false;
	/// Editor layer back-pointer for menus/dialogs that need editor-wide context.
	EditorLayer* mp_editorLayer{nullptr};
	/// Whether the canvas shows the tiles the generator would write.
	bool m_showGeneratorPreview{false};
	/// Result of the generator on the tilemap.
	std::optional<scene::pcg::Result> m_generatorPreview;
	/// Settings, graph file date and grid the preview was evaluated for.
	std::string m_generatorPreviewKey;
	/// Tile palette state (selected layer + brush).
	panel::TilePalette m_palette;

	/// Canvas zoom in pixels per cell (clamped at the call site).
	float m_zoomPixelsPerCell = 32.f;
	/// Canvas pan offset in pixels.
	math::vec2 m_panOffset{0.f, 0.f};
	/// Whether a paint stroke is currently active (between mouse-down and mouse-up).
	bool m_strokeInProgress = false;
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
