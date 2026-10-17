/**
 * @file NavMap.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "scene/nav/NavProfile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace owl::scene {

class Tileset;
class TilemapAsset;
}// namespace owl::scene

namespace owl::scene::nav {

/// Side of a navigation chunk, in cells: the one of the tilemap chunks, the unit of the change tracking.
inline constexpr uint32_t chunkSide = 32;

/// Number of cells of a chunk.
inline constexpr uint32_t chunkCellCount = chunkSide * chunkSide;

/// Number of cells on the border of a chunk.
inline constexpr uint32_t borderCellCount = 4 * chunkSide - 4;

/// Cost of an edge between two cells of cost 1, in the integer units of the searches (a cell costs 16).
inline constexpr uint32_t edgeUnit = 32;

/// Distance of a cell the search did not reach.
inline constexpr uint32_t unreachable = 0xFFFFFFFFu;

/**
 * @brief
 *  Navigation data of one chunk of cells, as arrays: what a search reads.
 */
struct OWL_API NavChunk {
	/// Cost of crossing each cell, in 1/16 cell; 0 for a blocked cell.
	std::array<uint16_t, chunkCellCount> cost{};
	/// Directions each cell can be left in (rule masks cut by the slopes).
	std::array<uint8_t, chunkCellCount> exits{};
	/// Directions each cell can be entered in (rule masks cut by the slopes).
	std::array<uint8_t, chunkCellCount> enters{};
	/// Rise of each cell along +x: sum of its two corners on the east edge minus the two on the west edge.
	std::array<int8_t, chunkCellCount> riseX{};
	/// Rise of each cell along +y: sum of its two corners on the south edge minus the two on the north edge.
	std::array<int8_t, chunkCellCount> riseY{};
};

/**
 * @brief
 *  Distances between the border cells of one chunk, filled on demand by the hierarchical search.
 *
 * Row `i` holds the cost of the best path inside the chunk from border cell `i` to each border cell.
 */
struct OWL_API NavRows {
	/// `borderCellCount²` costs, `unreachable` where no path stays in the chunk.
	std::vector<uint32_t> distance;
	/// Whether the row of each border cell is computed.
	std::vector<uint8_t> valid;
	/// For each row, the border indices it reaches (other than its own), `borderCellCount` slots per row.
	std::vector<uint16_t> targets;
	/// Number of reached border cells of each row.
	std::vector<uint16_t> count;
};

/**
 * @brief
 *  The graph of a tilemap: a copy of what the searches read, rebuilt chunk by chunk when the tilemap changes.
 *
 * The map is built from a `TilemapAsset` and a `NavProfile`. `sync` compares the generation of every chunk of the
 * layer (and of the corner heights) with the one it saw, and rebuilds only the chunks that changed, dropping the
 * cached border distances of those chunks alone. A chunk without a passable cell costs a null pointer.
 *
 * Not thread safe: the searches fill the border-distance cache of a const map.
 */
class OWL_API NavMap final {
public:
	NavMap();

	~NavMap();

	/**
	 * @brief
	 *  Copy constructor.
	 */
	NavMap(const NavMap&) = delete;

	/**
	 * @brief
	 *  Move constructor.
	 */
	NavMap(NavMap&&) noexcept;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const NavMap&) -> NavMap& = delete;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(NavMap&&) noexcept -> NavMap&;

	/**
	 * @brief
	 *  Change the rules; the next `sync` rebuilds every chunk.
	 * @param[in] iProfile The rules.
	 */
	void setProfile(const NavProfile& iProfile);

	/**
	 * @brief
	 *  The rules in use.
	 * @return The profile.
	 */
	[[nodiscard]] auto getProfile() const -> const NavProfile& { return m_profile; }

	/**
	 * @brief
	 *  Bring the graph up to date with a tilemap, rebuilding the chunks whose generation changed.
	 * @param[in] iAsset The tilemap.
	 * @return The number of chunks rebuilt.
	 */
	auto sync(const TilemapAsset& iAsset) -> uint32_t;

	/**
	 * @brief
	 *  Grid size.
	 * @return The size in cells (0 before the first `sync`).
	 */
	[[nodiscard]] auto getSize() const -> const math::vec2ui& { return m_size; }

	/**
	 * @brief
	 *  Size of the chunk grid.
	 * @return The number of chunks along each axis.
	 */
	[[nodiscard]] auto getChunkGridSize() const -> const math::vec2ui& { return m_chunkGrid; }

	/**
	 * @brief
	 *  Counter bumped by every change of the graph (a rebuilt chunk or new rules).
	 * @return The version.
	 */
	[[nodiscard]] auto getVersion() const -> uint64_t { return m_version; }

	/**
	 * @brief
	 *  Number of chunks rebuilt since the creation of the map.
	 * @return The count.
	 */
	[[nodiscard]] auto getRebuiltChunkCount() const -> uint64_t { return m_rebuiltChunks; }

	/**
	 * @brief
	 *  Number of chunks holding a passable cell.
	 * @return The count of chunks with data.
	 */
	[[nodiscard]] auto getAllocatedChunkCount() const -> size_t;

	/**
	 * @brief
	 *  Number of cells that are nodes of the graph.
	 * @return The count.
	 */
	[[nodiscard]] auto getWalkableCount() const -> uint64_t { return m_walkableCount; }

	/**
	 * @brief
	 *  What the tile of a cell opens, before the slopes and the exceptions of the profile.
	 *
	 * Usable once `sync` has run on the asset: it follows the mode of the profile (rules, tileset connections, any cell).
	 * @param[in] iAsset The tilemap.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return The cost and the sides of the cell (`tile` is the id read), nothing when the cell is not a node.
	 */
	[[nodiscard]] auto getBaseCell(const TilemapAsset& iAsset, uint32_t iX, uint32_t iY) const
			-> std::optional<NavTileRule>;

	/**
	 * @brief
	 *  Whether a cell is a node of the graph.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return False for a blocked cell or one outside the grid.
	 */
	[[nodiscard]] auto isWalkable(uint32_t iX, uint32_t iY) const -> bool;

	/**
	 * @brief
	 *  Cost of crossing a cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return The cost in cells (the base cost is 1), 0 for a blocked cell.
	 */
	[[nodiscard]] auto getCellCost(uint32_t iX, uint32_t iY) const -> float;

	/**
	 * @brief
	 *  Cost of the move from a cell to its neighbour.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iDirection Direction of the move.
	 * @return The cost in the integer units of the searches (`edgeUnit` is a move between two base cells), 0 when
	 *  the move does not exist (blocked cell, one-way, slope, outside the grid).
	 */
	[[nodiscard]] auto getEdgeCost(uint32_t iX, uint32_t iY, Direction iDirection) const -> uint32_t;

	/**
	 * @brief
	 *  Convert a cost of the searches to cells.
	 * @param[in] iUnits The cost in integer units.
	 * @return The cost in cells (a move between two base cells is 1).
	 */
	[[nodiscard]] static auto unitsToCells(uint32_t iUnits) -> float {
		return static_cast<float>(iUnits) / static_cast<float>(edgeUnit);
	}

	/**
	 * @brief
	 *  Lowest cost of a move of the graph, the unit of the heuristic of the searches.
	 * @return The cost in integer units (`edgeUnit` for base costs, at least 2).
	 */
	[[nodiscard]] auto getMinEdgeCost() const -> uint32_t { return m_minEdgeCost; }

	/**
	 * @brief
	 *  Data of one chunk.
	 * @param[in] iIndex Row-major index in the chunk grid.
	 * @return The chunk, nullptr when it has no passable cell or the index is outside the grid.
	 */
	[[nodiscard]] auto getChunk(size_t iIndex) const -> const NavChunk* {
		return iIndex < m_chunks.size() ? m_chunks[iIndex].get() : nullptr;
	}

	/**
	 * @brief
	 *  Border distances cache of one chunk, allocated on first use (search internals).
	 * @param[in] iIndex Row-major index in the chunk grid (inside it).
	 * @return The cache, reset when the chunk was rebuilt since the last use.
	 */
	[[nodiscard]] auto getRows(size_t iIndex) const -> NavRows&;

	/**
	 * @brief
	 *  Free the border distances caches (memory only: they are filled again on demand).
	 */
	void releaseSearchCaches() const;

	/**
	 * @brief
	 *  Index in the border of a cell of a chunk.
	 * @param[in] iLocal Row-major index of the cell inside the chunk.
	 * @return The border index, or -1 for a cell inside the chunk.
	 */
	[[nodiscard]] static auto getBorderIndex(uint32_t iLocal) -> int32_t;

	/**
	 * @brief
	 *  Cell index inside the chunk of the border cell of a given border index.
	 * @param[in] iBorder Border index (`< borderCellCount`).
	 * @return The row-major index inside the chunk.
	 */
	[[nodiscard]] static auto getBorderCell(uint32_t iBorder) -> uint32_t;

private:
	/**
	 * @brief
	 *  Rebuild the data of one chunk from the tilemap.
	 * @param[in] iAsset The tilemap.
	 * @param[in] iChunkX Chunk x.
	 * @param[in] iChunkY Chunk y.
	 */
	void rebuildChunk(const TilemapAsset& iAsset, uint32_t iChunkX, uint32_t iChunkY);

	/**
	 * @brief
	 *  What a tile id opens in the current mode.
	 * @param[in] iId Id stored in the cell (not empty).
	 * @return The rule of the id, nothing when it is not part of the graph.
	 */
	[[nodiscard]] auto resolveId(uint16_t iId) const -> std::optional<NavTileRule>;

	/**
	 * @brief
	 *  Read the connections of the tileset into `m_connectionOfTile`.
	 * @param[in] iTileset The tileset, may be null.
	 */
	void readConnections(const Tileset* iTileset);

	/**
	 * @brief
	 *  Forget everything built (size or rules changed).
	 */
	void reset();

	/// Stamp of a chunk never built (no real generation takes it).
	static constexpr uint64_t neverBuilt = ~0ull;

	/// Generations of the sources a chunk was built from.
	struct Stamp {
		/// Generation of the tile layer chunk.
		uint64_t tiles = neverBuilt;
		/// Generation of the corner heights around the chunk.
		uint64_t heights = neverBuilt;
	};

	/// The rules.
	NavProfile m_profile;
	/// Index into `m_profile.rules` plus one, per tile index; 0 for a tile without rule.
	std::vector<uint16_t> m_ruleOfTile;
	/// Connections mask of each tile index of the tileset, -1 for a tile without connections.
	std::vector<int16_t> m_connectionOfTile;
	/// Signature of the connections the table was read from.
	uint64_t m_connectionSignature = 0;
	/// Index into `m_profile.overrides` of the exception of each cell, by `y << 32 | x`.
	std::unordered_map<uint64_t, size_t> m_overrideOfCell;
	/// Grid size in cells.
	math::vec2ui m_size{0, 0};
	/// Chunk grid size.
	math::vec2ui m_chunkGrid{0, 0};
	/// Row-major chunk data, null for a chunk without passable cell.
	std::vector<uniq<NavChunk>> m_chunks;
	/// Row-major stamps of the sources of each chunk.
	std::vector<Stamp> m_stamps;
	/// Row-major number of nodes of each chunk.
	std::vector<uint16_t> m_chunkWalkable;
	/// Row-major version of each chunk (the map version at its last rebuild).
	std::vector<uint64_t> m_chunkVersions;
	/// Border distances caches, allocated on use.
	mutable std::vector<uniq<NavRows>> m_rows;
	/// Chunk version each cache was filled for.
	mutable std::vector<uint64_t> m_rowVersions;
	/// Map version.
	uint64_t m_version = 1;
	/// Number of nodes of the graph.
	uint64_t m_walkableCount = 0;
	/// Chunks rebuilt so far.
	uint64_t m_rebuiltChunks = 0;
	/// Lowest edge cost of the rules, in search units.
	uint32_t m_minEdgeCost = edgeUnit;
	/// Climb cost per rise unit, in search units.
	uint32_t m_climbUnits = 0;
	/// Whether the rules changed since the last `sync`.
	bool m_profileDirty = true;
};

}// namespace owl::scene::nav
