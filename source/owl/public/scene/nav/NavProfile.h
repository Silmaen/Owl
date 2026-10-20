/**
 * @file NavProfile.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

/**
 * @brief
 *  Tile-graph pathfinding: the rules turning a tilemap into a graph, the graph and the searches over it.
 */
namespace owl::scene::nav {

/// Move on the grid: the four neighbours of a cell, `East` along +x, `South` along +y.
enum struct Direction : uint8_t {
	East = 0,///< Towards x + 1.
	South = 1,///< Towards y + 1.
	West = 2,///< Towards x - 1.
	North = 3,///< Towards y - 1.
};

/// Number of directions.
inline constexpr uint32_t directionCount = 4;

/// Mask with every direction set.
inline constexpr uint8_t allDirections = 0x0F;

/// Every direction, in index order.
inline constexpr std::array<Direction, directionCount> directions{Direction::East, Direction::South, Direction::West,
																  Direction::North};

/// Name of each direction in the files (`east`, `south`, `west`, `north`), in index order.
inline constexpr std::array<const char*, directionCount> directionNames{"east", "south", "west", "north"};

/**
 * @brief
 *  Direction of a file name.
 * @param[in] iName One of `directionNames`.
 * @return The direction, nothing for an unknown name.
 */
[[nodiscard]] OWL_API auto parseDirection(std::string_view iName) -> std::optional<Direction>;

/**
 * @brief
 *  Bit of a direction in a direction mask.
 * @param[in] iDirection The direction.
 * @return `1 << index`.
 */
[[nodiscard]] constexpr auto directionBit(const Direction iDirection) -> uint8_t {
	return static_cast<uint8_t>(1u << static_cast<uint8_t>(iDirection));
}

/**
 * @brief
 *  The direction going back.
 * @param[in] iDirection The direction.
 * @return The opposite direction.
 */
[[nodiscard]] constexpr auto opposite(const Direction iDirection) -> Direction {
	return static_cast<Direction>((static_cast<uint8_t>(iDirection) + 2u) % directionCount);
}

/**
 * @brief
 *  Cell offset along x of a direction.
 * @param[in] iDirection The direction.
 * @return -1, 0 or 1.
 */
[[nodiscard]] constexpr auto offsetX(const Direction iDirection) -> int32_t {
	constexpr std::array<int32_t, directionCount> table{1, 0, -1, 0};
	return table[static_cast<size_t>(iDirection)];
}

/**
 * @brief
 *  Cell offset along y of a direction.
 * @param[in] iDirection The direction.
 * @return -1, 0 or 1.
 */
[[nodiscard]] constexpr auto offsetY(const Direction iDirection) -> int32_t {
	constexpr std::array<int32_t, directionCount> table{0, 1, 0, -1};
	return table[static_cast<size_t>(iDirection)];
}

/// Which tile of a cell the graph reads.
enum struct NavSource : uint8_t {
	Overlay = 0,///< The overlay tile: only the cells carrying a listed overlay (roads, tracks) are nodes.
	Terrain = 1,///< The terrain tile: the cells of a listed terrain are nodes (walking on a grid).
};

/// How the graph is read from the tilemap.
enum struct NavMode : uint8_t {
	Rules = 0,///< Explicit per-tile rules (no rule, no graph).
	Connections = 1,///< The `connections` the tileset gives to each tile (roads), the way to author a graph.
	AnyCell = 2,///< Every cell with a tile is a node open on its four sides (walking on a grid).
};

/// State of one side of a cell once the exceptions are applied.
enum struct SideState : uint8_t {
	TwoWay = 0,///< Movers go through the side both ways.
	ExitOnly = 1,///< Movers can leave the cell by this side but not come in by it.
	EnterOnly = 2,///< Movers can come in by this side but not leave by it.
	Closed = 3,///< Nobody goes through the side.
};

/// Name of each `SideState` in the commands, in index order.
inline constexpr std::array<const char*, 4> sideStateNames{"two_way", "exit_only", "enter_only", "closed"};

/**
 * @brief
 *  The next state when a side is clicked: two-way, exit only, enter only, closed, then two-way again.
 * @param[in] iState The current state.
 * @return The following state of the cycle.
 */
[[nodiscard]] constexpr auto nextSideState(const SideState iState) -> SideState {
	return static_cast<SideState>((static_cast<uint8_t>(iState) + 1u) % 4u);
}

/**
 * @brief
 *  The exceptions of one cell: sides closed to leaving or entering, and its own cost.
 *
 * An exception only restricts what the tile (or the rules) allow; it never opens a side the tile keeps closed.
 */
struct OWL_API NavCellOverride {
	/// Cell x.
	uint32_t x = 0;
	/// Cell y.
	uint32_t y = 0;
	/// Sides (`directionBit` of the neighbour direction) the cell cannot be left by.
	uint8_t noExit = 0;
	/// Sides (`directionBit` of the neighbour direction) the cell cannot be entered from.
	uint8_t noEnter = 0;
	/// Cost of crossing the cell, in cells; 0 keeps the cost of its tile.
	float cost = 0.f;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other exception.
	 * @return True when both hold the same values.
	 */
	auto operator==(const NavCellOverride& iOther) const -> bool = default;
	OWL_DIAG_POP

	/**
	 * @brief
	 *  Whether the exception changes nothing.
	 * @return True without any closed side and without cost.
	 */
	[[nodiscard]] auto isNeutral() const -> bool { return noExit == 0 && noEnter == 0 && cost <= 0.f; }

	/**
	 * @brief
	 *  State of one side of the cell.
	 * @param[in] iSide The side, as the direction of the neighbour.
	 * @return The state.
	 */
	[[nodiscard]] auto getSide(Direction iSide) const -> SideState;

	/**
	 * @brief
	 *  Change the state of one side of the cell.
	 * @param[in] iSide The side, as the direction of the neighbour.
	 * @param[in] iState The new state.
	 */
	void setSide(Direction iSide, SideState iState);
};

/**
 * @brief
 *  How one tile takes part in the graph.
 */
struct OWL_API NavTileRule {
	/// Tile index the rule applies to.
	int32_t tile = 0;
	/// Cost of crossing a cell with this tile, in cells (1 is the base cost, kept between 1/16 and 64).
	float cost = 1.f;
	/// Directions of travel the cell can be left in (`directionBit` mask).
	uint8_t exits = allDirections;
	/// Directions of travel the cell can be entered in: a mover heading East enters it when `East` is set.
	uint8_t enters = allDirections;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other rule.
	 * @return True when both rules hold the same values.
	 */
	auto operator==(const NavTileRule& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  The rules turning a tilemap into a graph of cells: which tile makes a node, what it costs, which ways it opens,
 *  and how steep a slope a move may cross.
 *
 * A move from a cell to its neighbour exists when both cells have a rule, the first one lets the mover leave in that
 * direction, the second one lets it enter in that direction, and no slope on the way is too steep. A road that is
 * one-way lists only the direction of travel in `exits`.
 */
struct OWL_API NavProfile {
	/// How the graph is read (the tileset connections, any cell, or the explicit `rules`).
	NavMode mode = NavMode::Rules;
	/// Layer of the tilemap the graph is read from.
	uint32_t layer = 0;
	/// Tile of the layer the rules apply to.
	NavSource source = NavSource::Overlay;
	/// One rule per tile that is part of the graph (the other cells are blocked); used by `NavMode::Rules` only.
	std::vector<NavTileRule> rules;
	/// Exceptions of single cells (one-way, closed sides, slower cells), at most one per cell.
	std::vector<NavCellOverride> overrides;
	/// Highest rise, in height steps, a move may cross over one cell (0 keeps the paths on flat ground).
	uint8_t maxRise = 1;
	/// Extra cost, in cells, of climbing one height step.
	float climbCost = 0.5f;
	/// Whether a cell on a steep slope (a corner two steps above the lowest) can be crossed.
	bool allowSteep = false;

	/// Highest value of `maxRise`.
	static constexpr uint8_t maxRiseLimit = 4;

	/**
	 * @brief
	 *  Whether the profile builds no graph (explicit rules mode without any rule).
	 * @return True when there is no graph.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return mode == NavMode::Rules && rules.empty(); }

	/**
	 * @brief
	 *  Exception of a cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @return The exception, nullptr when the cell has none.
	 */
	[[nodiscard]] auto findOverride(uint32_t iX, uint32_t iY) const -> const NavCellOverride*;

	/**
	 * @brief
	 *  State of the side of a cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iSide The side, as the direction of the neighbour.
	 * @return The state, two-way without exception.
	 */
	[[nodiscard]] auto getSideState(uint32_t iX, uint32_t iY, Direction iSide) const -> SideState;

	/**
	 * @brief
	 *  Set the state of the edge between a cell and its neighbour, on both cells so that they agree.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iSide The side of the cell, as the direction of the neighbour.
	 * @param[in] iState State seen from the cell (`ExitOnly`: movers leave the cell towards the neighbour only).
	 */
	void setEdgeState(uint32_t iX, uint32_t iY, Direction iSide, SideState iState);

	/**
	 * @brief
	 *  Set the cost of a cell.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iCost Cost in cells, 0 (or less) to keep the cost of the tile.
	 */
	void setCellCost(uint32_t iX, uint32_t iY, float iCost);

	/**
	 * @brief
	 *  Rule of a tile.
	 * @param[in] iTile Tile index.
	 * @return The rule, or nullptr when the tile is not part of the graph.
	 */
	[[nodiscard]] auto findRule(int32_t iTile) const -> const NavTileRule*;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other profile.
	 * @return True when both profiles hold the same values.
	 */
	auto operator==(const NavProfile& iOther) const -> bool = default;
	OWL_DIAG_POP
};

}// namespace owl::scene::nav
