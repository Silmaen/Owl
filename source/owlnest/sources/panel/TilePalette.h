/**
 * @file TilePalette.h
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::scene {

class TilemapAsset;
}// namespace owl::scene

namespace owl::nest::panel {
/// Sentinel brush index meaning "nothing selected — clicking on the canvas picks the cell".
constexpr int32_t g_TileBrushPick = -2;
/// Sentinel brush index meaning "eraser — clicking on the canvas writes `g_EmptyTileIndex`".
constexpr int32_t g_TileBrushEraser = -1;

/**
 * @brief
 *  Tile selection panel for tilemap authoring.
 *
 * Stateless brush picker — the panel itself only owns the *current* layer
 * index and **two** independent brushes:
 *
 * - **Primary brush** — applied by left mouse on the canvas.
 * - **Secondary brush** — applied by right mouse on the canvas.
 *
 * Each brush is either `g_TileBrushPick` (no selection — left/right click on
 * the canvas picks the tile under the cursor and assigns it to that brush),
 * `g_TileBrushEraser` (writes `g_EmptyTileIndex`) or a non-negative tile
 * index. Re-clicking the same selection in the palette toggles the brush
 * back to pick mode.
 *
 * The hosting document feeds the active `TilemapAsset` on every frame; the
 * panel renders the asset's tileset as a clickable atlas grid. Left-click on
 * an atlas cell drives the primary brush; right-click drives the secondary
 * one. The eraser button works the same way (left = primary eraser, right =
 * secondary eraser).
 */
class TilePalette final {
public:
	TilePalette() = default;

	~TilePalette() = default;

	TilePalette(const TilePalette&) = delete;

	TilePalette(TilePalette&&) = delete;

	auto operator=(const TilePalette&) -> TilePalette& = delete;

	auto operator=(TilePalette&&) -> TilePalette& = delete;

	/**
	 * @brief
	 *  Render the panel against the given asset. Pass `nullptr` to show the empty state.
	 * @param[in] iTarget The tilemap asset whose tileset and layers drive the panel.
	 */
	void onImGuiRender(scene::TilemapAsset* iTarget);

	/**
	 * @brief
	 *  Currently active primary brush (left mouse on the canvas).
	 * @return One of `g_TileBrushPick`, `g_TileBrushEraser`, or a non-negative tile index.
	 */
	[[nodiscard]] auto getPrimaryBrush() const -> int32_t { return m_primaryBrush; }

	/**
	 * @brief
	 *  Currently active secondary brush (right mouse on the canvas).
	 * @return One of `g_TileBrushPick`, `g_TileBrushEraser`, or a non-negative tile index.
	 */
	[[nodiscard]] auto getSecondaryBrush() const -> int32_t { return m_secondaryBrush; }

	/**
	 * @brief
	 *  Set the primary brush (used after a canvas pick to assign the picked tile).
	 * @param[in] iBrush The new brush value.
	 */
	void setPrimaryBrush(int32_t iBrush) { m_primaryBrush = iBrush; }

	/**
	 * @brief
	 *  Set the secondary brush (used after a canvas pick to assign the picked tile).
	 * @param[in] iBrush The new brush value.
	 */
	void setSecondaryBrush(int32_t iBrush) { m_secondaryBrush = iBrush; }

	/**
	 * @brief
	 *  Currently active painting layer (0-based index into the asset's layers).
	 * @return The layer index.
	 */
	[[nodiscard]] auto getSelectedLayer() const -> uint32_t { return m_selectedLayer; }

	/**
	 * @brief
	 *  Force the active layer index (clamped at the call site).
	 * @param[in] iIndex The 0-based layer index.
	 */
	void setSelectedLayer(uint32_t iIndex) { m_selectedLayer = iIndex; }

	/**
	 * @brief
	 *  Currently inspected tile (for the per-tile metadata editor in the Properties panel).
	 *  `-1` when no tile is selected.
	 * @return The selected tile index.
	 */
	[[nodiscard]] auto getInspectedTile() const -> int32_t { return m_inspectedTile; }

	/**
	 * @brief
	 *  Force the inspected tile.
	 * @param[in] iTile The new inspected tile index, or `-1` to clear.
	 */
	void setInspectedTile(int32_t iTile) { m_inspectedTile = iTile; }

private:
	/// Index of the active layer to paint into (0 = first / back).
	uint32_t m_selectedLayer = 0;
	/// Primary brush (left mouse on the canvas).
	int32_t m_primaryBrush = g_TileBrushPick;
	/// Secondary brush (right mouse on the canvas).
	int32_t m_secondaryBrush = g_TileBrushEraser;
	/// Tile inspected for the metadata editor (-1 = none).
	int32_t m_inspectedTile = -1;
};

}// namespace owl::nest::panel
