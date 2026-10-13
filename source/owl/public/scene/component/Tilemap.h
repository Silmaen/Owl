/**
 * @file Tilemap.h
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace owl::scene {

class TilemapAsset;
}// namespace owl::scene

namespace owl::scene::component {
/// Sentinel index meaning "no tile in this cell". Stored in the cells of a `TilemapLayer`.
constexpr int32_t g_EmptyTileIndex = -1;

/**
 * @brief
 *  One layer of a `TilemapAsset`.
 *
 * Holds its own `width × height` grid of tile indices into the asset's
 * shared tileset. Layers stack back-to-front in the order they are stored on
 * the asset (index 0 renders first, last index renders on top). Each layer
 * has an independent visibility flag and parallax factor used at render time.
 *
 * The parallax factor is a per-axis multiplier applied to the camera position
 * when the layer is drawn: `(1, 1)` is the default (moves with the world),
 * `(0.5, 0.5)` scrolls slower than the world (typical background), `(0, 0)`
 * is camera-locked (sky / fullscreen overlay).
 *
 * The cells are encapsulated: every write goes through `setTile`, `resize`,
 * `assign` or `fill`, which stamp the touched `chunkSize × chunkSize` chunks
 * with a fresh generation. Generations come from one process-wide counter, so a
 * generation value names one chunk content: a renderer caching a chunk rebuilds
 * it only when the generation it saw changes (a copy keeps its generations,
 * which still describe its content).
 *
 * Lives inside `scene::component::*` to keep the type in the existing
 * include path; the actual data container is `scene::TilemapAsset`.
 */
struct OWL_API TilemapLayer {
	/// Side of a square chunk, in cells: the unit of change tracking and of GPU caching.
	static constexpr uint32_t chunkSize = 32;

	/// Designer-friendly layer name (e.g. "background", "ground", "props").
	std::string name;
	/// Whether the layer is rendered.
	bool visible = true;
	/// Per-axis parallax factor (1 = move with world, 0 = camera-locked).
	math::vec2 parallax{1.f, 1.f};

	/**
	 * @brief
	 *  Default constructor: an empty `0 × 0` grid.
	 */
	TilemapLayer() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~TilemapLayer() = default;

	/**
	 * @brief
	 *  Copy constructor (generations are copied with the cells they describe).
	 */
	TilemapLayer(const TilemapLayer&) = default;

	/**
	 * @brief
	 *  Move constructor; the source is left as an empty `0 × 0` grid.
	 * @param[in,out] ioOther The layer to move from.
	 */
	TilemapLayer(TilemapLayer&& ioOther) noexcept;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const TilemapLayer&) -> TilemapLayer& = default;

	/**
	 * @brief
	 *  Move assignment operator; the source is left as an empty `0 × 0` grid.
	 * @param[in,out] ioOther The layer to move from.
	 * @return A reference to this object.
	 */
	auto operator=(TilemapLayer&& ioOther) noexcept -> TilemapLayer&;

	/**
	 * @brief
	 *  Resize the grid, keeping the cells that fit and filling new ones with `g_EmptyTileIndex`.
	 * @param[in] iSize The new grid size in cells.
	 */
	void resize(const math::vec2ui& iSize);

	/**
	 * @brief
	 *  Replace the whole grid.
	 * @param[in] iSize The grid size in cells.
	 * @param[in] iTiles Row-major tile indices; missing cells are empty, extra ones ignored.
	 */
	void assign(const math::vec2ui& iSize, std::span<const int32_t> iTiles);

	/**
	 * @brief
	 *  Set every cell of the grid to one value.
	 * @param[in] iValue Tile index, or `g_EmptyTileIndex`.
	 */
	void fill(int32_t iValue);

	/**
	 * @brief
	 *  Grid size in cells.
	 * @return The size (`x` = width, `y` = height).
	 */
	[[nodiscard]] auto getSize() const -> const math::vec2ui& { return m_size; }

	/**
	 * @brief
	 *  Read one cell.
	 * @param[in] iX Cell x (0-based, left to right).
	 * @param[in] iY Cell y (0-based, top to bottom).
	 * @return The tile index, or `g_EmptyTileIndex` outside the grid.
	 */
	[[nodiscard]] auto getTile(uint32_t iX, uint32_t iY) const -> int32_t;

	/**
	 * @brief
	 *  Write one cell; a changed value stamps its chunk with a new generation.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iValue Tile index, or `g_EmptyTileIndex` to clear.
	 * @return True when the cell is inside the grid and its value changed.
	 */
	auto setTile(uint32_t iX, uint32_t iY, int32_t iValue) -> bool;

	/**
	 * @brief
	 *  Read-only view of every cell.
	 * @return The row-major tile indices (`width × height` entries).
	 */
	[[nodiscard]] auto getTiles() const -> std::span<const int32_t> { return m_tiles; }

	/**
	 * @brief
	 *  Number of non-empty cells.
	 * @return The count of cells holding a tile.
	 */
	[[nodiscard]] auto getOccupiedCount() const -> size_t;

	/**
	 * @brief
	 *  Number of chunks along each axis (partial chunks at the right and bottom edges included).
	 * @return The chunk grid size.
	 */
	[[nodiscard]] auto getChunkGridSize() const -> math::vec2ui;

	/**
	 * @brief
	 *  Generation of one chunk.
	 * @param[in] iChunk Chunk coordinates (`x` right, `y` down).
	 * @return The generation, or 0 outside the chunk grid.
	 */
	[[nodiscard]] auto getChunkGeneration(const math::vec2ui& iChunk) const -> uint64_t;

	/**
	 * @brief
	 *  Newest generation stamped on any chunk of this layer.
	 * @return The layer generation (0 for a layer never written).
	 */
	[[nodiscard]] auto getGeneration() const -> uint64_t { return m_generation; }

private:
	/**
	 * @brief
	 *  Stamp every chunk with one new generation (the chunk grid follows `m_size`).
	 */
	void restampAll();

	/// Grid size in cells.
	math::vec2ui m_size{0, 0};
	/// Row-major flat grid of tile indices into the tileset; `g_EmptyTileIndex` = empty.
	std::vector<int32_t> m_tiles;
	/// Row-major generation of each chunk.
	std::vector<uint64_t> m_chunkGenerations;
	/// Newest generation stamped on this layer.
	uint64_t m_generation = 0;
};

/**
 * @brief
 *  Reference to a `TilemapAsset` (`.owltilemap`) attached to an entity.
 *
 * Since v0.2.0 the grid data (width / height / layers / tileset) lives in a
 * standalone `scene::TilemapAsset` so the same level can be reused across
 * scenes and authored in a dedicated tilemap document. This component is the
 * scene-side handle: a relative path on disk plus the runtime-resolved
 * asset shared pointer. The path is what gets serialized; the asset is
 * resolved on scene load (mirrors the texture-loading pattern used by
 * `SpriteRenderer`).
 *
 * When the path is empty but `asset` is non-null (e.g. an in-memory tilemap
 * built programmatically by tests, or a legacy inline tilemap that has not
 * yet been saved through the editor), the asset's data is serialized inline
 * under the `inline:` key for back-compat. The supported path is the
 * preferred form for new scenes.
 *
 * On-disk form (preferred):
 * ```yaml
 * Tilemap:
 *   tilemapPath: assets/tilemaps/dungeon_l1.owltilemap
 * ```.
 *
 * On-disk form (legacy / in-memory fallback):
 * ```yaml
 * Tilemap:
 *   inline:
 *     tilesetPath: assets/tilesets/dungeon.owltileset
 *     width: 32
 *     height: 16
 *     cellSize: 1.0
 *     layers:
 *       - { name: ground, tiles: "0,0,1,1,..." }
 * ```.
 */
struct OWL_API Tilemap {
	/// Relative path (from the project root) to the `.owltilemap` asset.
	std::filesystem::path tilemapPath;
	/// Resolved asset (runtime only — built from the path or from inline data on load).
	shared<scene::TilemapAsset> asset;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Tilemap"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Tilemap"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 *
	 * Accepts both the preferred `tilemapPath`-only form and the legacy inline
	 * form (either flat or nested under `inline:`). Inline data is moved into
	 * a fresh in-memory `TilemapAsset`; the path is left empty and the asset
	 * is considered "unsaved" until the user gives it a path through the
	 * tilemap editor.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
