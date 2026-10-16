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
	/// Layer of the tilemap the graph is read from.
	uint32_t layer = 0;
	/// Tile of the layer the rules apply to.
	NavSource source = NavSource::Overlay;
	/// One rule per tile that is part of the graph (the other cells are blocked).
	std::vector<NavTileRule> rules;
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
	 *  Whether the profile has no rule (no graph).
	 * @return True without any rule.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return rules.empty(); }

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
