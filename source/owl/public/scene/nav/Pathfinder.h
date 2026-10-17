/**
 * @file Pathfinder.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "scene/nav/NavMap.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace owl::scene::nav {

/// Algorithm of a search.
enum struct PathMode : uint8_t {
	Auto = 0,///< Flat A*, except hierarchical for cells 3 chunks apart or more on a sparse graph (at most 1/5 of the cells).
	Flat = 1,///< A* over the cells.
	Hierarchical = 2,///< A* over the border cells of the chunks, then each chunk crossed is refined.
};

/**
 * @brief
 *  Constraints on the moves of a search, for a vehicle that keeps its heading.
 *
 * With `noReverse` the search is heading-aware (its state is the cell and the direction it was entered by): a move back
 * to the cell just left is forbidden, except from a dead end (a cell with a single exit). The search is always flat:
 * `PathMode::Hierarchical` and `Auto` fall back to the flat algorithm. Without `noReverse` the options are ignored.
 */
struct PathOptions {
	/// Forbid the move back to the cell just left, except from a dead end.
	bool noReverse = false;
	/// Direction the mover already goes in at the start cell: its first move cannot reverse it (unless a dead end).
	std::optional<Direction> heading;
};

/// Where a search stands.
enum struct PathState : uint8_t {
	Idle = 0,///< No search started.
	Searching = 1,///< `step` has more work to do.
	Found = 2,///< A path is ready to be taken.
	NotFound = 3,///< No path joins the cells (or one of them is not a node of the graph).
};

/**
 * @brief
 *  A path found by a search.
 */
struct OWL_API PathResult {
	/// The cells from the start to the goal, both included.
	std::vector<math::vec2ui> cells;
	/// Cost of the path, in the integer units of the searches (`NavMap::unitsToCells` converts it).
	uint32_t cost = 0;

	/**
	 * @brief
	 *  Cost of the path in cells.
	 * @return The cost (a move between two base cells is 1).
	 */
	[[nodiscard]] auto getCostInCells() const -> float { return NavMap::unitsToCells(cost); }
};

/**
 * @brief
 *  Shortest path between two cells of a `NavMap`, as a search that can be cut in slices.
 *
 * Both algorithms are exact: they return paths of the same cost. The flat one runs A* over the cells. The
 * hierarchical one runs A* over the border cells of the 32x32 chunks, where a move inside a chunk costs the
 * distance of the best path that stays in it (computed once per chunk and border cell, cached in the `NavMap` until
 * the chunk changes), then refines each crossed chunk into cells.
 *
 * `begin` starts a search and `step` works a given number of node expansions further, so a game spreads a long
 * query over several frames. The working arrays are kept between searches: a search allocates nothing once the
 * arrays reached the size of the map and the result vector the length of the path. One search at a time.
 */
class OWL_API Pathfinder final {
public:
	Pathfinder();

	~Pathfinder();

	/**
	 * @brief
	 *  Copy constructor.
	 */
	Pathfinder(const Pathfinder&) = delete;

	/**
	 * @brief
	 *  Move constructor.
	 */
	Pathfinder(Pathfinder&&) = delete;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const Pathfinder&) -> Pathfinder& = delete;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(Pathfinder&&) -> Pathfinder& = delete;

	/**
	 * @brief
	 *  Start a search, dropping the one in progress.
	 *
	 * The map must stay alive and unchanged until the search ends (`getMapVersion` tells when it changed).
	 * @param[in] iMap The graph, already synchronised with its tilemap.
	 * @param[in] iFrom The start cell.
	 * @param[in] iTo The goal cell.
	 * @param[in] iMode The algorithm.
	 * @param[in] iOptions Constraints on the moves (`noReverse` makes the search flat and heading-aware).
	 * @return The state after the start: `Searching`, or `NotFound` right away for a cell that is not a node.
	 */
	auto begin(const NavMap& iMap, const math::vec2ui& iFrom, const math::vec2ui& iTo, PathMode iMode = PathMode::Auto,
			   const PathOptions& iOptions = {}) -> PathState;

	/**
	 * @brief
	 *  Work on the search until about a number of nodes are expanded.
	 * @param[in] iBudget Node expansions to spend (a slice is not cut inside the expansion of one chunk, so it can
	 *  overrun by up to a chunk of cells).
	 * @return The state after the slice.
	 */
	auto step(uint32_t iBudget) -> PathState;

	/**
	 * @brief
	 *  State of the search.
	 * @return The state.
	 */
	[[nodiscard]] auto getState() const -> PathState { return m_state; }

	/**
	 * @brief
	 *  Hand the path of a finished search over.
	 *
	 * The cell vectors of the result and of the pathfinder are swapped, so the buffers are reused.
	 * @param[in,out] ioResult The result to fill.
	 * @return False (and nothing changed) when the state is not `Found`.
	 */
	auto takeResult(PathResult& ioResult) -> bool;

	/**
	 * @brief
	 *  Run a whole search.
	 * @param[in] iMap The graph, already synchronised with its tilemap.
	 * @param[in] iFrom The start cell.
	 * @param[in] iTo The goal cell.
	 * @param[in] iMode The algorithm.
	 * @param[out] oResult The path when found.
	 * @param[in] iOptions Constraints on the moves.
	 * @return True when a path was found.
	 */
	auto find(const NavMap& iMap, const math::vec2ui& iFrom, const math::vec2ui& iTo, PathMode iMode,
			  PathResult& oResult, const PathOptions& iOptions = {}) -> bool;

	/**
	 * @brief
	 *  Version of the map when the search began.
	 * @return The `NavMap::getVersion` value.
	 */
	[[nodiscard]] auto getMapVersion() const -> uint64_t { return m_mapVersion; }

	/**
	 * @brief
	 *  Node expansions spent by the last search.
	 * @return The count.
	 */
	[[nodiscard]] auto getWorkDone() const -> uint64_t { return m_work; }

private:
	/// Entry of the open list: ordered by `f`, then by the larger `g`.
	struct OpenNode {
		uint32_t f = 0;
		uint32_t g = 0;
		uint32_t node = 0;
	};

	/// Entry of the open list of a search inside one chunk.
	struct LocalNode {
		uint32_t distance = 0;
		uint32_t cell = 0;
	};

	/// How an abstract node was reached.
	enum struct Kind : uint8_t { Seed, Intra, Cross, Goal, Direct, Step };

	/// Steps of a hierarchical search (`Heading`: the flat search of a mover that cannot reverse).
	enum struct Phase : uint8_t { Flat, StartChunk, GoalChunk, Abstract, Refine, Heading };

	/// Entry direction of the start state of a heading search with no heading, and number of entry states per cell.
	static constexpr uint32_t noHeading = directionCount;

	/**
	 * @brief
	 *  Heuristic of a cell: the Manhattan distance to the goal at the lowest edge cost.
	 * @param[in] iNode Global cell index.
	 * @return The estimate.
	 */
	[[nodiscard]] auto heuristic(uint32_t iNode) const -> uint32_t;

	/**
	 * @brief
	 *  Record a better way to a node and queue it.
	 * @param[in] iNode Node (a cell index, or `m_goalNode`).
	 * @param[in] iCost Cost of the way.
	 * @param[in] iParent Previous node.
	 * @param[in] iKind How the node is reached.
	 */
	void relax(uint32_t iNode, uint32_t iCost, uint32_t iParent, Kind iKind);

	/**
	 * @brief
	 *  Expand nodes of the flat search.
	 * @param[in] iBudget Expansions to spend.
	 */
	void stepFlat(uint32_t iBudget);

	/**
	 * @brief
	 *  Expand states of the heading-aware search (state = cell * 5 + the direction of the move into it).
	 * @param[in] iBudget Expansions to spend.
	 */
	void stepHeading(uint32_t iBudget);

	/**
	 * @brief
	 *  Record a better way to a state of the heading-aware search and queue it.
	 * @param[in] iState State.
	 * @param[in] iCost Cost of the way.
	 * @param[in] iParent Previous state.
	 */
	void relaxState(uint32_t iState, uint32_t iCost, uint32_t iParent);

	/**
	 * @brief
	 *  Expand nodes of the abstract search.
	 * @param[in] iBudget Expansions to spend.
	 */
	void stepAbstract(uint32_t iBudget);

	/**
	 * @brief
	 *  Whether a move leaves the chunk of a cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return True when a neighbour in another chunk can be reached from the cell.
	 */
	[[nodiscard]] auto hasCrossExit(uint32_t iX, uint32_t iY) const -> bool;

	/**
	 * @brief
	 *  Expand the border cell of a chunk: its distances inside the chunk (when entered from outside), the moves
	 *  out of the chunk and the end of the path.
	 * @param[in] iNode Global cell index of the border cell.
	 */
	void expandBorder(uint32_t iNode);

	/**
	 * @brief
	 *  Dijkstra from a cell over the cells of its chunk.
	 * @param[in] iChunkX Chunk x.
	 * @param[in] iChunkY Chunk y.
	 * @param[in] iStart Row-major index of the start inside the chunk.
	 * @param[in] iReverse False: distances from the start; true: distances to the start (moves taken backwards).
	 * @param[out] oDistance Distance of each cell of the chunk.
	 * @param[out] oDirection Direction of the last move into the cell (forward) or of the first one out of it
	 *  (reverse).
	 */
	void searchChunk(uint32_t iChunkX, uint32_t iChunkY, uint32_t iStart, bool iReverse,
					 std::array<uint32_t, chunkCellCount>& oDistance, std::array<uint8_t, chunkCellCount>& oDirection);

	/**
	 * @brief
	 *  Distances between the border cells of a chunk, from one of them (cached in the map).
	 * @param[in] iChunkX Chunk x.
	 * @param[in] iChunkY Chunk y.
	 * @param[in] iLocal Row-major index of the border cell inside the chunk.
	 * @param[out] oRows The cache holding the row.
	 * @param[out] oIndex Border index of the cell (the row in `oRows`).
	 */
	void borderRow(uint32_t iChunkX, uint32_t iChunkY, uint32_t iLocal, NavRows*& oRows, size_t& oIndex);

	/**
	 * @brief
	 *  Turn the abstract path found into the list of nodes to refine.
	 */
	void prepareRefine();

	/**
	 * @brief
	 *  Refine one piece of the abstract path into cells.
	 */
	void stepRefine();

	/**
	 * @brief
	 *  Finish a flat search: walk the parents back from the goal.
	 */
	void finishFlat();

	/**
	 * @brief
	 *  Append the cells of a path inside a chunk, from the cell where the search started to a cell.
	 * @param[in] iDirection Direction of the move into each cell, from a forward search.
	 * @param[in] iChunkX Chunk x.
	 * @param[in] iChunkY Chunk y.
	 * @param[in] iStart Local index of the start of the search.
	 * @param[in] iEnd Local index of the last cell.
	 * @param[in] iSkipFirst Whether the start cell is already in the path.
	 */
	void appendForward(const std::array<uint8_t, chunkCellCount>& iDirection, uint32_t iChunkX, uint32_t iChunkY,
					   uint32_t iStart, uint32_t iEnd, bool iSkipFirst);

	/**
	 * @brief
	 *  Move the stamp to a new search, clearing the arrays on wrap.
	 */
	void newStamp();

	/// The graph.
	const NavMap* mp_map = nullptr;
	/// Map version at the start.
	uint64_t m_mapVersion = 0;
	/// State.
	PathState m_state = PathState::Idle;
	/// Step of the search.
	Phase m_phase = Phase::Flat;
	/// Grid size.
	uint32_t m_width = 0;
	/// Cells of the grid (also the index of the virtual goal node of the abstract search).
	uint32_t m_goalNode = 0;
	/// Start cell index.
	uint32_t m_from = 0;
	/// Goal cell index.
	uint32_t m_to = 0;
	/// Goal cell x.
	uint32_t m_toX = 0;
	/// Goal cell y.
	uint32_t m_toY = 0;
	/// Lowest edge cost of the map.
	uint32_t m_minEdge = edgeUnit;
	/// Search stamp: an array entry is valid when its stamp equals this one.
	uint32_t m_stamp = 0;
	/// Cost of the path found.
	uint32_t m_total = 0;
	/// Node expansions of the search.
	uint64_t m_work = 0;
	/// Stamp of each node.
	std::vector<uint32_t> m_visit;
	/// Best cost to each node.
	std::vector<uint32_t> m_cost;
	/// Previous node of each node.
	std::vector<uint32_t> m_parent;
	/// How each node was reached (a `Kind`, or the direction of the move in a flat search).
	std::vector<uint8_t> m_kind;
	/// Stamp of each state of the heading-aware search.
	std::vector<uint32_t> m_stateVisit;
	/// Best cost to each state of the heading-aware search.
	std::vector<uint32_t> m_stateCost;
	/// Previous state of each state of the heading-aware search.
	std::vector<uint32_t> m_stateParent;
	/// Open list.
	std::vector<OpenNode> m_open;
	/// Open list of the searches inside a chunk.
	std::vector<LocalNode> m_localOpen;
	/// Distances from the start inside its chunk.
	std::array<uint32_t, chunkCellCount> m_startDistance{};
	/// Directions of the start search.
	std::array<uint8_t, chunkCellCount> m_startDirection{};
	/// Distances to the goal inside its chunk.
	std::array<uint32_t, chunkCellCount> m_goalDistance{};
	/// Directions of the goal search.
	std::array<uint8_t, chunkCellCount> m_goalDirection{};
	/// Distances of a refining or border search.
	std::array<uint32_t, chunkCellCount> m_scratchDistance{};
	/// Directions of a refining or border search.
	std::array<uint8_t, chunkCellCount> m_scratchDirection{};
	/// Border cells of the abstract path, from the start.
	std::vector<uint32_t> m_chain;
	/// Next piece of the chain to refine.
	size_t m_refineIndex = 0;
	/// The path being built.
	std::vector<math::vec2ui> m_cells;
};

/**
 * @brief
 *  Whether every step of a path still exists in the graph.
 * @param[in] iMap The graph.
 * @param[in] iCells The cells of the path, start to goal.
 * @return True when each cell is the neighbour of the previous one and the move between them exists (a path of one
 *  cell is valid when that cell is a node; an empty one is not valid).
 */
[[nodiscard]] OWL_API auto isPathValid(const NavMap& iMap, std::span<const math::vec2ui> iCells) -> bool;

}// namespace owl::scene::nav
