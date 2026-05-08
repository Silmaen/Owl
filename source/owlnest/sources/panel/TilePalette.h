/**
 * @file TilePalette.h
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest::panel {
/**
 * @brief
 *  Tile selection panel for tilemap authoring.
 *
 * Renders the tileset atlas of the currently selected `Tilemap` entity as a clickable
 * grid: clicking a slot selects that tile index for painting; pressing the eraser
 * button selects "no tile" (`-1`). The currently active layer index is also surfaced
 * here so the user can switch between layers without leaving the panel.
 *
 * The panel does **not** drive the actual painting — it only owns the brush state.
 * The viewport reads `getSelectedTile()` and `getSelectedLayer()` to know what to
 * paint when the user clicks inside a tilemap entity's grid.
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
	 *  Set the currently inspected entity (read-only; null when no Tilemap).
	 * @param[in] iEntity The current scene-hierarchy selection.
	 */
	void setSelectedEntity(const scene::Entity& iEntity) { m_selection = iEntity; }

	/**
	 * @brief
	 *  Render the panel.
	 */
	void onImGuiRender();

	/**
	 * @brief
	 *  Currently selected tile index, or `-1` when the eraser is active.
	 * @return The brush tile index.
	 */
	[[nodiscard]] auto getSelectedTile() const -> int32_t { return m_selectedTile; }

	/**
	 * @brief
	 *  Currently active painting layer (0-based index into the tilemap's layers).
	 * @return The layer index.
	 */
	[[nodiscard]] auto getSelectedLayer() const -> uint32_t { return m_selectedLayer; }

	/**
	 * @brief
	 *  Whether a paint-capable tilemap selection is active.
	 *
	 * True when the selection has a `Tilemap` component, with a resolved `tileset`,
	 * a valid layer index, and a brush tile (paint or erase).
	 * @return True when the viewport should accept paint clicks.
	 */
	[[nodiscard]] auto isPaintActive() const -> bool;

private:
	/// Currently selected scene entity (mirrored from the hierarchy panel).
	scene::Entity m_selection;
	/// Index of the active layer to paint into (0 = first / back).
	uint32_t m_selectedLayer = 0;
	/// Brush tile index. `-1` = eraser.
	int32_t m_selectedTile = -1;
};

}// namespace owl::nest::panel
