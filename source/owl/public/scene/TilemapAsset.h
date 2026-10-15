/**
 * @file TilemapAsset.h
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/FormatVersion.h"
#include "math/vectors.h"
#include "scene/component/Tilemap.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene {

class Tileset;

/// One corner whose height a terraforming operation changed.
struct OWL_API CornerHeightChange {
	/// Index in the corner grid (`y * (width + 1) + x`).
	uint32_t index = 0;
	/// Height before the operation.
	uint8_t before = 0;
	/// Height after the operation.
	uint8_t after = 0;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other change.
	 * @return True when both changes are the same.
	 */
	auto operator==(const CornerHeightChange& iOther) const -> bool = default;
};

/**
 * @brief
 *  Reusable tilemap level asset (`.owltilemap`).
 *
 * Extracts the grid data of a tilemap (size, layers, per-cell tile indices,
 * tileset reference) into a standalone authoring document so the same level
 * can be referenced by multiple scenes and edited in a dedicated tilemap
 * editor instead of being inlined into a single `.owl` scene.
 *
 * The on-disk format (version 3) is a YAML header, the YAML end-of-document line `...`, then a binary payload
 * holding the non-empty chunks of every layer and the corner heights of the isometric terrain (little-endian,
 * checksummed, see `doc/pages/scene.md`):
 * ```yaml
 * Tilemap: <name>
 * FormatVersion: 3
 * tilesetPath: assets/tilesets/dungeon.owltileset
 * width: 32
 * height: 16
 * cellSize: 1.0
 * layers:
 *   - name: ground
 *     visible: false
 *     parallax: [0.5, 0.5]
 *   - name: props
 * payloadBytes: 10268
 * ...
 * <binary payload>
 * ```.
 *
 * Version 1 files (pure YAML, one comma-separated `tiles` string per layer) and version 2 files (no heights) load
 * through the migration chain and are written back as version 3.
 *
 * The isometric terrain is a `(width + 1) x (height + 1)` grid of corner heights (steps of
 * `terrain::heightStep`), empty while the map is flat. Corners next to each other never differ by more than one
 * step, so every cell has one of the 19 slopes of `scene/TilemapTerrain.h`; the terraforming calls keep that true.
 *
 * The `TilemapLayer` value type is reused from
 * `scene::component::Tilemap` — both the inline-on-component path
 * (transitional) and this asset format share the same in-memory
 * representation.
 */
class OWL_API TilemapAsset final {
public:
	TilemapAsset() = default;

	~TilemapAsset() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	TilemapAsset(const TilemapAsset&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	TilemapAsset(TilemapAsset&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const TilemapAsset&) -> TilemapAsset& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(TilemapAsset&&) -> TilemapAsset& = default;

	/**
	 * @brief
	 *  File extension used by `.owltilemap` assets (with leading dot).
	 * @return The extension literal (`".owltilemap"`).
	 */
	static auto fileExtension() noexcept -> const char* { return ".owltilemap"; }

	/// Largest grid width or height: a file announcing more is refused, `resize` clamps to it.
	static constexpr uint32_t maxGridSide = 16384;

	/**
	 * @brief
	 *  Format descriptor of the `.owltilemap` files, with its migration chain.
	 * @return The TilemapAsset format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/// Relative path (from the project root) to the `.owltileset` asset.
	std::filesystem::path tilesetPath;
	/// Resolved tileset (runtime only, not serialized).
	shared<scene::Tileset> tileset;
	/// Grid width in cells (>= 1).
	uint32_t width = 16;
	/// Grid height in cells (>= 1).
	uint32_t height = 16;
	/// World-space size of one cell (uniform; tile aspect is handled by the renderer).
	float cellSize = 1.f;
	/// Ordered list of layers (back-to-front).
	std::vector<scene::component::TilemapLayer> layers;

	/**
	 * @brief
	 *  Resize all existing layers to a new grid shape, padding with empty.
	 *
	 * Existing tile data is preserved for cells that fit in the new bounds; new
	 * cells are filled with `g_EmptyTileIndex`. Width / height are clamped to
	 * `[1, maxGridSide]`.
	 * @param[in] iWidth New grid width (>= 1).
	 * @param[in] iHeight New grid height (>= 1).
	 */
	void resize(uint32_t iWidth, uint32_t iHeight);

	/**
	 * @brief
	 *  Append a new empty layer with the given name.
	 *
	 * The new layer is created `width × height` cells, all empty, visible, and
	 * with `(1, 1)` parallax. Returns a reference to the new layer.
	 * @param[in] iName The layer name.
	 * @return The newly created layer.
	 */
	auto addLayer(const std::string& iName) -> scene::component::TilemapLayer&;

	/**
	 * @brief
	 *  Get a tile index from a layer.
	 * @param[in] iLayer The 0-based layer index.
	 * @param[in] iX Cell x (0-based, left to right).
	 * @param[in] iY Cell y (0-based, top to bottom).
	 * @return The tile index, or `g_EmptyTileIndex` for out-of-grid cells.
	 */
	[[nodiscard]] auto getTile(uint32_t iLayer, uint32_t iX, uint32_t iY) const -> int32_t;

	/**
	 * @brief
	 *  Set a tile in a layer.
	 *
	 * Out-of-grid cells are silently ignored. Out-of-range layer indices are a no-op.
	 * @param[in] iLayer The 0-based layer index.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iValue Tile index, or `g_EmptyTileIndex` to clear.
	 */
	void setTile(uint32_t iLayer, uint32_t iX, uint32_t iY, int32_t iValue);

	/// Highest corner height.
	static constexpr uint8_t maxCornerHeight = 255;

	/**
	 * @brief
	 *  Whether the terrain has any corner above zero (the corner grid is allocated).
	 * @return False for a flat map.
	 */
	[[nodiscard]] auto hasHeights() const -> bool { return !m_cornerHeights.empty(); }

	/**
	 * @brief
	 *  The corner heights, row by row.
	 * @return The `(width + 1) * (height + 1)` heights, or an empty span for a flat map.
	 */
	[[nodiscard]] auto getCornerHeights() const -> std::span<const uint8_t> { return m_cornerHeights; }

	/**
	 * @brief
	 *  Highest corner height of the map.
	 * @return The height in steps (0 for a flat map).
	 */
	[[nodiscard]] auto getMaxCornerHeight() const -> uint8_t { return m_maxCornerHeight; }

	/**
	 * @brief
	 *  Height of one corner.
	 * @param[in] iX Corner x, `[0, width]`.
	 * @param[in] iY Corner y, `[0, height]`.
	 * @return The height in steps (0 outside the grid).
	 */
	[[nodiscard]] auto getCornerHeight(uint32_t iX, uint32_t iY) const -> uint8_t;

	/**
	 * @brief
	 *  Heights of the four corners of a cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return The heights, back, right, front, left (all 0 outside the grid).
	 */
	[[nodiscard]] auto getCellCorners(uint32_t iX, uint32_t iY) const -> std::array<uint8_t, 4>;

	/**
	 * @brief
	 *  Slope of a cell, derived from its corners.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return The slope, `[0, terrain::slopeCount)`; flat outside the grid.
	 */
	[[nodiscard]] auto getCellSlope(uint32_t iX, uint32_t iY) const -> uint8_t;

	/**
	 * @brief
	 *  Height of the lowest corner of a cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return The height in steps.
	 */
	[[nodiscard]] auto getCellBaseHeight(uint32_t iX, uint32_t iY) const -> uint8_t;

	/**
	 * @brief
	 *  Set the height of one corner, bringing the neighbours back within one step of it.
	 * @param[in] iX Corner x, `[0, width]`.
	 * @param[in] iY Corner y, `[0, height]`.
	 * @param[in] iHeight New height in steps.
	 * @return The corners that changed (empty when nothing moved or the corner is outside the grid).
	 */
	auto setCornerHeight(uint32_t iX, uint32_t iY, uint8_t iHeight) -> std::vector<CornerHeightChange>;

	/**
	 * @brief
	 *  Raise one corner by one step.
	 * @param[in] iX Corner x.
	 * @param[in] iY Corner y.
	 * @return The corners that changed.
	 */
	auto raiseCorner(uint32_t iX, uint32_t iY) -> std::vector<CornerHeightChange>;

	/**
	 * @brief
	 *  Lower one corner by one step.
	 * @param[in] iX Corner x.
	 * @param[in] iY Corner y.
	 * @return The corners that changed.
	 */
	auto lowerCorner(uint32_t iX, uint32_t iY) -> std::vector<CornerHeightChange>;

	/**
	 * @brief
	 *  Raise every corner of an area by one step.
	 * @param[in] iFirst First corner (clamped to the grid).
	 * @param[in] iLast Last corner, inclusive.
	 * @return The corners that changed.
	 */
	auto raiseArea(const math::vec2ui& iFirst, const math::vec2ui& iLast) -> std::vector<CornerHeightChange>;

	/**
	 * @brief
	 *  Lower every corner of an area by one step.
	 * @param[in] iFirst First corner (clamped to the grid).
	 * @param[in] iLast Last corner, inclusive.
	 * @return The corners that changed.
	 */
	auto lowerArea(const math::vec2ui& iFirst, const math::vec2ui& iLast) -> std::vector<CornerHeightChange>;

	/**
	 * @brief
	 *  Bring every corner of an area to the same height.
	 * @param[in] iFirst First corner (clamped to the grid).
	 * @param[in] iLast Last corner, inclusive.
	 * @param[in] iHeight The height in steps.
	 * @return The corners that changed.
	 */
	auto levelArea(const math::vec2ui& iFirst, const math::vec2ui& iLast, uint8_t iHeight)
			-> std::vector<CornerHeightChange>;

	/**
	 * @brief
	 *  Play corner changes back, or undo them.
	 * @param[in] iChanges The changes of a terraforming call.
	 * @param[in] iUndo True to put the `before` heights back, false to apply the `after` ones.
	 */
	void applyCornerChanges(std::span<const CornerHeightChange> iChanges, bool iUndo);

	/**
	 * @brief
	 *  Replace the whole corner grid.
	 *
	 * Corners that break the one-step rule are raised until it holds again.
	 * @param[in] iHeights `(width + 1) * (height + 1)` heights, or empty for a flat map.
	 * @return False (and nothing changed) when the size does not match the grid.
	 */
	auto setCornerHeights(std::vector<uint8_t> iHeights) -> bool;

	/**
	 * @brief
	 *  Memory used by the cells of every layer.
	 * @return The size in bytes (`TilemapLayer::getStorageBytes` summed).
	 */
	[[nodiscard]] auto getStorageBytes() const -> size_t;

	/**
	 * @brief
	 *  Whether two assets hold the same tileset path, grid, cell size and layers (the resolved tileset ignored).
	 * @param[in] iOther The other asset.
	 * @return True when the assets have the same content.
	 */
	[[nodiscard]] auto hasSameContent(const TilemapAsset& iOther) const -> bool;

	/**
	 * @brief
	 *  Serialize the tilemap asset in the `.owltilemap` format (YAML header and binary payload).
	 * @param[in] iName Optional display name written under the `Tilemap:` key.
	 * @return The file content (binary after the header).
	 */
	[[nodiscard]] auto serializeToString(std::string_view iName = "") const -> std::string;

	/**
	 * @brief
	 *  Populate the tilemap asset from a `.owltilemap` content, version 1 (YAML) or 2 (YAML header and binary).
	 * @param[in] iText The file content.
	 * @return True on success, false on malformed, truncated or corrupted input (asset left unchanged).
	 */
	[[nodiscard]] auto deserializeFromString(std::string_view iText) -> bool;


	/**
	 * @brief
	 *  Save the tilemap asset to disk. Atomic: a failed write keeps the previous file.
	 * @param[in] iPath Destination file (`.owltilemap` is conventional).
	 * @param[in] iName Optional display name; defaults to the file stem.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath, std::string_view iName = "") const -> bool;

	/**
	 * @brief
	 *  Load the tilemap asset from disk.
	 * @param[in] iPath Source file.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadFromFile(const std::filesystem::path& iPath) -> bool;

private:
	/**
	 * @brief
	 *  Index of a corner in the height grid.
	 * @param[in] iX Corner x.
	 * @param[in] iY Corner y.
	 * @return The position in `m_cornerHeights`.
	 */
	[[nodiscard]] auto getCornerIndex(uint32_t iX, uint32_t iY) const -> size_t {
		return static_cast<size_t>(iY) * (width + 1) + iX;
	}

	/**
	 * @brief
	 *  Move a corner and its neighbours, recording the changes.
	 */
	void flattenAround(uint32_t iX, uint32_t iY, uint8_t iHeight, std::vector<CornerHeightChange>& ioChanges);

	/**
	 * @brief
	 *  Raise (+1), lower (-1) or level (0, to `iLevel`) every corner of an area.
	 */
	auto terraformArea(const math::vec2ui& iFirst, const math::vec2ui& iLast, int iDelta, uint8_t iLevel)
			-> std::vector<CornerHeightChange>;

	/**
	 * @brief
	 *  Recompute the highest corner and drop the grid when it is all zero.
	 */
	void refreshHeightSummary();

	/**
	 * @brief
	 *  Raise corners until neighbours differ by at most one step.
	 */
	void repairHeights();

	/// Corner heights, row by row, empty for a flat map.
	std::vector<uint8_t> m_cornerHeights;
	/// Highest value of `m_cornerHeights`.
	uint8_t m_maxCornerHeight = 0;
};

}// namespace owl::scene
