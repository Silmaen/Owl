/**
 * @file Tilemap.h
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"
#include "scene/nav/NavProfile.h"

#include <array>
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

/// Highest tile index a cell can hold (cells store 16-bit ids, `0xFFFF` being the empty one).
constexpr int32_t g_MaxTileIndex = 0xFFFE;

/**
 * @brief
 *  Content of one cell: a terrain tile, an overlay tile drawn over it and 8 bits of flags.
 *
 * The tile indices use the `int32_t` convention of the tilemap API (`g_EmptyTileIndex` for none, at most
 * `g_MaxTileIndex`). The renderer draws the terrain; the overlay and the flags are stored and saved for the
 * isometric terrain (tracks, roads) and for games, the engine does not interpret them yet.
 */
struct TileCell {
	/// Terrain tile index, or `g_EmptyTileIndex`.
	int32_t terrain = g_EmptyTileIndex;
	/// Overlay tile index, or `g_EmptyTileIndex`.
	int32_t overlay = g_EmptyTileIndex;
	/// Free bits attached to the cell.
	uint8_t flags = 0;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other cell.
	 * @return True when both cells hold the same values.
	 */
	auto operator==(const TileCell& iOther) const -> bool = default;
};

/**
 * @brief
 *  Storage of one `TilemapChunk::side`² block of cells, as a structure of arrays.
 *
 * Cells are row-major inside the chunk. Ids are 16-bit, `emptyId` meaning no tile. The cells of a chunk lying
 * outside the grid of its layer (right and bottom edges) are always empty.
 */
struct OWL_API TilemapChunk {
	/// Side of a chunk, in cells.
	static constexpr uint32_t side = 32;
	/// Number of cells in a chunk.
	static constexpr uint32_t cellCount = side * side;
	/// Stored id of an empty cell.
	static constexpr uint16_t emptyId = 0xFFFF;

	/// Terrain id of each cell.
	std::array<uint16_t, cellCount> terrain{};
	/// Overlay id of each cell.
	std::array<uint16_t, cellCount> overlay{};
	/// Flags of each cell.
	std::array<uint8_t, cellCount> flags{};

	/**
	 * @brief
	 *  Default constructor: every cell empty.
	 */
	TilemapChunk();

	/**
	 * @brief
	 *  Whether every cell is empty (no terrain, no overlay, no flag).
	 * @return True for an empty chunk.
	 */
	[[nodiscard]] auto isEmpty() const -> bool;

	/**
	 * @brief
	 *  Read one cell.
	 * @param[in] iIndex Row-major index inside the chunk (`< cellCount`).
	 * @return The cell.
	 */
	[[nodiscard]] auto getCell(uint32_t iIndex) const -> TileCell;

	/**
	 * @brief
	 *  Write one cell.
	 * @param[in] iIndex Row-major index inside the chunk (`< cellCount`).
	 * @param[in] iCell The cell; tile indices outside `[0, g_MaxTileIndex]` are stored as empty.
	 */
	void setCell(uint32_t iIndex, const TileCell& iCell);

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other chunk.
	 * @return True when both chunks hold the same cells.
	 */
	auto operator==(const TilemapChunk& iOther) const -> bool = default;
};

/**
 * @brief
 *  Shared, read-only handle on the stored content of one chunk.
 *
 * Layers share their chunks between copies and copy a chunk only when one of them writes into it, so a handle
 * taken from a layer stays valid and unchanged whatever happens to the layer afterwards. Undo commands keep the
 * chunks they replace this way: an edit of one cell costs one chunk. A null handle stands for an empty chunk.
 */
class OWL_API TilemapChunkRef final {
public:
	/**
	 * @brief
	 *  The chunk content.
	 * @return The chunk, or nullptr for an empty chunk.
	 */
	[[nodiscard]] auto get() const -> const TilemapChunk* { return m_chunk.get(); }

	/**
	 * @brief
	 *  Whether the handle points at stored content.
	 * @return False for an empty chunk.
	 */
	[[nodiscard]] explicit operator bool() const { return m_chunk != nullptr; }

	/**
	 * @brief
	 *  Identity comparison: both handles point at the same stored chunk.
	 * @param[in] iOther The other handle.
	 * @return True when the handles share their chunk (or are both null).
	 */
	auto operator==(const TilemapChunkRef& iOther) const -> bool { return m_chunk == iOther.m_chunk; }

private:
	friend struct TilemapLayer;
	/// The shared chunk, never written while another owner holds it.
	shared<TilemapChunk> m_chunk;
};

/**
 * @brief
 *  One layer of a `TilemapAsset`.
 *
 * Holds its own `width × height` grid of cells (`TileCell`) indexing the asset's shared tileset. Layers stack
 * back-to-front in the order they are stored on the asset (index 0 renders first, last index renders on top).
 * Each layer has an independent visibility flag and parallax factor used at render time.
 *
 * The parallax factor is a per-axis multiplier applied to the camera position
 * when the layer is drawn: `(1, 1)` is the default (moves with the world),
 * `(0.5, 0.5)` scrolls slower than the world (typical background), `(0, 0)`
 * is camera-locked (sky / fullscreen overlay).
 *
 * The cells live in `chunkSize × chunkSize` chunks (`TilemapChunk`, a structure of arrays) allocated on the first
 * write: a chunk never written costs a pointer, so the storage follows the painted area, not the grid size. Copies
 * of a layer share their chunks until one of them writes (copy on write). No call returns or takes the whole grid
 * as one array but `assign`, a convenience for small maps.
 *
 * The cells are encapsulated: every write goes through `setTile`, `setCell`, `setChunk`, `setChunkRef`, `resize`,
 * `assign` or `fill`, which stamp the touched chunks with a fresh generation. Generations come from one
 * process-wide counter, so a generation value names one chunk content: a renderer caching a chunk rebuilds it only
 * when the generation it saw changes (a copy keeps its generations, which still describe its content).
 *
 * Lives inside `scene::component::*` to keep the type in the existing
 * include path; the actual data container is `scene::TilemapAsset`.
 */
struct OWL_API TilemapLayer {
	/// Side of a square chunk, in cells: the unit of storage, of change tracking and of GPU caching.
	static constexpr uint32_t chunkSize = TilemapChunk::side;

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
	 *  Copy constructor (the chunks are shared until written, generations are copied with them).
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
	 *  Resize the grid, keeping the cells that fit and leaving new ones empty.
	 * @param[in] iSize The new grid size in cells.
	 */
	void resize(const math::vec2ui& iSize);

	/**
	 * @brief
	 *  Replace the whole grid with terrain tiles (overlays and flags cleared).
	 * @param[in] iSize The grid size in cells.
	 * @param[in] iTiles Row-major terrain indices; missing cells are empty, extra ones ignored.
	 */
	void assign(const math::vec2ui& iSize, std::span<const int32_t> iTiles);

	/**
	 * @brief
	 *  Set the terrain of every cell to one value (overlays and flags cleared).
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
	 *  Read the terrain tile of one cell.
	 * @param[in] iX Cell x (0-based, left to right).
	 * @param[in] iY Cell y (0-based, top to bottom).
	 * @return The tile index, or `g_EmptyTileIndex` outside the grid.
	 */
	[[nodiscard]] auto getTile(uint32_t iX, uint32_t iY) const -> int32_t;

	/**
	 * @brief
	 *  Write the terrain tile of one cell; a changed value stamps its chunk with a new generation.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iValue Tile index (at most `g_MaxTileIndex`), or `g_EmptyTileIndex` to clear.
	 * @return True when the cell is inside the grid, the value valid and changed.
	 */
	auto setTile(uint32_t iX, uint32_t iY, int32_t iValue) -> bool;

	/**
	 * @brief
	 *  Read one whole cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return The cell, empty outside the grid.
	 */
	[[nodiscard]] auto getCell(uint32_t iX, uint32_t iY) const -> TileCell;

	/**
	 * @brief
	 *  Write one whole cell; a change stamps its chunk with a new generation.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iCell The cell (tile indices at most `g_MaxTileIndex`, or `g_EmptyTileIndex`).
	 * @return True when the cell is inside the grid, the values valid and the cell changed.
	 */
	auto setCell(uint32_t iX, uint32_t iY, const TileCell& iCell) -> bool;

	/**
	 * @brief
	 *  Number of non-empty terrain cells.
	 * @return The count of cells holding a terrain tile.
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
	 *  Stored content of one chunk.
	 * @param[in] iChunk Chunk coordinates (`x` right, `y` down).
	 * @return The chunk, or nullptr for a chunk never written (empty) or outside the chunk grid.
	 */
	[[nodiscard]] auto getChunk(const math::vec2ui& iChunk) const -> const TilemapChunk*;

	/**
	 * @brief
	 *  Shared handle on the content of one chunk, which later writes to the layer never change.
	 * @param[in] iChunk Chunk coordinates.
	 * @return The handle, null for an empty chunk or outside the chunk grid.
	 */
	[[nodiscard]] auto getChunkRef(const math::vec2ui& iChunk) const -> TilemapChunkRef;

	/**
	 * @brief
	 *  Put back a chunk content taken by `getChunkRef` (from this layer or another one).
	 *
	 * Cells of the chunk lying outside the grid are cleared (on a copy when needed). Stamps the chunk with a new
	 * generation.
	 * @param[in] iChunk Chunk coordinates.
	 * @param[in] iRef The content; a null handle empties the chunk.
	 * @return True when the chunk is inside the chunk grid.
	 */
	auto setChunkRef(const math::vec2ui& iChunk, const TilemapChunkRef& iRef) -> bool;

	/**
	 * @brief
	 *  Replace the content of one chunk by a copy of `iContent`.
	 *
	 * Cells of the chunk lying outside the grid are cleared; an empty chunk frees its storage. Stamps the chunk
	 * with a new generation.
	 * @param[in] iChunk Chunk coordinates.
	 * @param[in] iContent The new content.
	 * @return True when the chunk is inside the chunk grid.
	 */
	auto setChunk(const math::vec2ui& iChunk, const TilemapChunk& iContent) -> bool;

	/**
	 * @brief
	 *  Number of chunks holding storage.
	 * @return The count of allocated chunks.
	 */
	[[nodiscard]] auto getAllocatedChunkCount() const -> size_t;

	/**
	 * @brief
	 *  Memory used by the cells of this layer: the allocated chunks plus the chunk tables.
	 * @return The size in bytes.
	 */
	[[nodiscard]] auto getStorageBytes() const -> size_t;

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

	/**
	 * @brief
	 *  Whether two layers hold the same name, visibility, parallax, size and cells (generations ignored).
	 * @param[in] iOther The other layer.
	 * @return True when the layers have the same content.
	 */
	[[nodiscard]] auto hasSameContent(const TilemapLayer& iOther) const -> bool;

private:
	/**
	 * @brief
	 *  Stamp every chunk with one new generation (the chunk grid follows `m_size`).
	 */
	void restampAll();

	/**
	 * @brief
	 *  Stamp one chunk with a new generation.
	 * @param[in] iIndex Chunk index in the table.
	 */
	void stamp(size_t iIndex);

	/**
	 * @brief
	 *  Index of a chunk in the chunk table.
	 * @param[in] iChunk Chunk coordinates, inside the chunk grid.
	 * @return The row-major index.
	 */
	[[nodiscard]] auto chunkIndex(const math::vec2ui& iChunk) const -> size_t;

	/**
	 * @brief
	 *  Chunk of the table owned by this layer alone, allocated empty or copied (when shared) first.
	 * @param[in] iIndex Chunk index in the table.
	 * @return The writable chunk.
	 */
	auto writableChunk(size_t iIndex) -> TilemapChunk&;

	/**
	 * @brief
	 *  Whether a chunk holds cells outside the grid that are not empty.
	 * @param[in] iChunk Chunk coordinates.
	 * @param[in] iContent The chunk content.
	 * @return True when some cell beyond the right or bottom edge is not empty.
	 */
	[[nodiscard]] auto hasCellsOutside(const math::vec2ui& iChunk, const TilemapChunk& iContent) const -> bool;

	/**
	 * @brief
	 *  Clear the cells of a chunk lying outside the grid.
	 * @param[in] iChunk Chunk coordinates.
	 * @param[in,out] ioContent The chunk content.
	 */
	void clearOutside(const math::vec2ui& iChunk, TilemapChunk& ioContent) const;

	/// Grid size in cells.
	math::vec2ui m_size{0, 0};
	/// Row-major chunk table; null for an empty chunk, shared between copies until written.
	std::vector<shared<TilemapChunk>> m_chunks;
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
 * A `navigation` map next to the path holds the pathfinding options (`scene/nav/NavProfile.h`). The roads come from the
 * `connections` the tileset gives to its tiles; `overrides` hold the exceptions edited on the isometric view (sides
 * closed to leaving or entering, a cell's own cost):
 * ```yaml
 * Tilemap:
 *   tilemapPath: assets/tilemaps/town.owltilemap
 *   navigation:
 *     mode: connections        # connections | cells | rules (a file without `mode` is read as `rules`)
 *     layer: 1
 *     source: overlay
 *     maxRise: 1
 *     climbCost: 0.5
 *     allowSteep: false
 *     overrides:
 *       - {cell: [12, 7], noExit: [north], noEnter: [south], cost: 2}
 * ```.
 *
 * The legacy `rules` list (`mode: rules`) still loads:
 * ```yaml
 *     rules:
 *       - {tile: 3, cost: 1.0, exits: [east, south, west, north], enters: [east, south, west, north]}
 * ```.
 *
 * On-disk form (legacy / in-memory fallback, terrain tiles only):
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
	/// Options turning the tilemap into a graph for the pathfinding (rules mode without rule: no graph).
	scene::nav::NavProfile navigation;

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
