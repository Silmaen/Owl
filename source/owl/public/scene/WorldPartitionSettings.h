/**
 * @file WorldPartitionSettings.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <string>
#include <vector>

namespace owl::scene {

/**
 * @brief
 *  One cell of the world partition: a scene merged into the running one while a streaming source is near it.
 */
struct OWL_API WorldCell {
	/// Integer coordinates of the cell: it spans `[coords, coords + 1) * cellSize` in world coordinates.
	math::vec3i coords{0, 0, 0};
	/// The `.owl` scene of the cell; its roots are placed relative to the corner of the cell.
	std::string scene;

	/**
	 * @brief
	 *  Field-wise comparison.
	 * @param[in] iOther The cell to compare with.
	 * @return True when both cells are the same.
	 */
	auto operator==(const WorldCell& iOther) const -> bool = default;
};

/**
 * @brief
 *  Per-scene world partition, saved in the scene file under `WorldPartition:`.
 *
 * The world is cut into cubic cells on world coordinates (double); in Play, the `owl.world_streaming` system loads the
 * scene of each cell nearer than `loadRadius` to a streaming source in the background (the task system), merges it
 * into the running scene, and unloads it once every source is farther than `unloadRadius`.
 */
struct OWL_API WorldPartitionSettings {
	/// Smallest accepted cell size, in world units.
	static constexpr double minCellSize = 1.0;

	/// Stream the cells at runtime.
	bool enabled = false;
	/// Side of a cell, in world units.
	double cellSize = 256.0;
	/// A cell loads once a source is nearer than this to it, in world units.
	double loadRadius = 384.0;
	/// A loaded cell unloads once every source is farther than this from it (at least `loadRadius`).
	double unloadRadius = 512.0;
	/// The cells that hold a scene, at most one per coordinates.
	std::vector<WorldCell> cells;

	/**
	 * @brief
	 *  Copy of these settings with every field brought into its accepted range and the duplicated cells removed (the
	 *  first one kept).
	 * @return The clamped settings.
	 */
	[[nodiscard]] auto clamped() const -> WorldPartitionSettings;

	/**
	 * @brief
	 *  Field-wise comparison.
	 * @param[in] iOther The settings to compare with.
	 * @return True when all fields are equal.
	 */
	auto operator==(const WorldPartitionSettings& iOther) const -> bool;

	/**
	 * @brief
	 *  The cell at some coordinates.
	 * @param[in] iCoords The coordinates.
	 * @return The cell, or null when none is declared there.
	 */
	[[nodiscard]] auto findCell(const math::vec3i& iCoords) const -> const WorldCell*;

	/**
	 * @brief
	 *  Coordinates of the cell holding a world position.
	 * @param[in] iPosition The world position.
	 * @return The cell coordinates.
	 */
	[[nodiscard]] auto getCellOf(const math::vec3d& iPosition) const -> math::vec3i;

	/**
	 * @brief
	 *  World position of the lowest corner of a cell.
	 * @param[in] iCoords The cell coordinates.
	 * @return The corner.
	 */
	[[nodiscard]] auto getCellCorner(const math::vec3i& iCoords) const -> math::vec3d;

	/**
	 * @brief
	 *  Distance from a world position to the nearest point of a cell (0 inside it).
	 * @param[in] iCoords The cell coordinates.
	 * @param[in] iPosition The world position.
	 * @return The distance, in world units.
	 */
	[[nodiscard]] auto getDistanceToCell(const math::vec3i& iCoords, const math::vec3d& iPosition) const -> double;
};

}// namespace owl::scene
