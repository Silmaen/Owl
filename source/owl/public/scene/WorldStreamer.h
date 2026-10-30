/**
 * @file WorldStreamer.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "WorldPartitionSettings.h"
#include "core/Core.h"
#include "core/UUID.h"
#include "math/vectors.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <span>
#include <vector>

namespace owl::core::task {
class Scheduler;
}// namespace owl::core::task

namespace owl::scene {

class Scene;
struct ParsedScene;

/**
 * @brief
 *  A point the cells are streamed around.
 */
struct OWL_API StreamPoint {
	/// World position, in double.
	math::vec3d position{0.0, 0.0, 0.0};
	/// Factor on the load and unload radii.
	double radiusScale = 1.0;
};

/// Where a cell is in its streaming.
enum struct WorldCellState : uint8_t {
	Unloaded,///< Not in the scene.
	Loading,///< Read and parsed in the background.
	Loaded,///< Its entities are in the scene.
	Failed///< Its scene could not be read or merged; retried once no source wants it any more.
};

/**
 * @brief
 *  Runtime of the world partition of a scene: which cells are loaded, the loads in flight, the entities each cell
 *  brought.
 *
 * The cell scenes are read and parsed on the workers of the task scheduler (on the calling thread without one), then
 * merged on the main thread by `SceneSerializer::mergeParsed`, at most `g_mergeBudget` per update so that a frame
 * never merges a whole region at once.
 */
class OWL_API WorldStreamer final {
public:
	/// Counters since the streamer was made.
	struct Stats {
		/// Cells loaded now.
		uint32_t loaded = 0;
		/// Cells being loaded now.
		uint32_t loading = 0;
		/// Loads started.
		uint64_t loadsStarted = 0;
		/// Cells merged.
		uint64_t merges = 0;
		/// Cells unloaded.
		uint64_t unloads = 0;
		/// Cells that failed to load.
		uint64_t failures = 0;
	};

	/// Cells merged per update at most.
	static constexpr uint32_t g_mergeBudget = 2;

	/**
	 * @brief
	 *  An empty streamer.
	 */
	WorldStreamer();

	/**
	 * @brief
	 *  Destructor.
	 */
	~WorldStreamer();

	/**
	 * @brief
	 *  Not copyable.
	 */
	WorldStreamer(const WorldStreamer&) = delete;

	/**
	 * @brief
	 *  Not movable (the workers hold its inbox).
	 */
	WorldStreamer(WorldStreamer&&) = delete;

	/**
	 * @brief
	 *  Not copyable.
	 * @return This streamer.
	 */
	auto operator=(const WorldStreamer&) -> WorldStreamer& = delete;

	/**
	 * @brief
	 *  Not movable.
	 * @return This streamer.
	 */
	auto operator=(WorldStreamer&&) -> WorldStreamer& = delete;

	/**
	 * @brief
	 *  Cells a set of sources wants loaded: the declared cells nearer than `loadRadius` (times the scale of a source).
	 * @param[in] iSettings The partition settings.
	 * @param[in] iSources The sources.
	 * @return The coordinates of the wanted cells, sorted.
	 */
	[[nodiscard]] static auto getWantedCells(const WorldPartitionSettings& iSettings,
											 std::span<const StreamPoint> iSources) -> std::vector<math::vec3i>;

	/**
	 * @brief
	 *  Stream for a frame: start the loads of the wanted cells, merge the finished ones, unload the cells every source
	 *  left (farther than `unloadRadius`). A disabled partition unloads everything.
	 * @param[in,out] ioScene The scene the cells merge into (its world origin places them).
	 * @param[in] iSources The sources.
	 * @param[in,out] ioScheduler The scheduler reading the cells, or null to read them on the calling thread.
	 */
	void update(Scene& ioScene, std::span<const StreamPoint> iSources, core::task::Scheduler* ioScheduler);

	/**
	 * @brief
	 *  Unload every cell.
	 * @param[in,out] ioScene The scene.
	 */
	void unloadAll(Scene& ioScene);

	/**
	 * @brief
	 *  Block until the loads in flight are parsed (tests, captures); they are merged by the next updates.
	 * @param[in,out] ioScheduler The scheduler they were given to, or null.
	 */
	static void waitForLoads(core::task::Scheduler* ioScheduler);

	/**
	 * @brief
	 *  State of a cell.
	 * @param[in] iCoords The cell coordinates.
	 * @return Its state.
	 */
	[[nodiscard]] auto getState(const math::vec3i& iCoords) const -> WorldCellState;

	/**
	 * @brief
	 *  Coordinates of the loaded cells.
	 * @return The cells, sorted.
	 */
	[[nodiscard]] auto getLoadedCells() const -> std::vector<math::vec3i>;

	/**
	 * @brief
	 *  Identifiers of the entities a loaded cell brought.
	 * @param[in] iCoords The cell coordinates.
	 * @return The identifiers, empty when the cell is not loaded.
	 */
	[[nodiscard]] auto getCellEntities(const math::vec3i& iCoords) const -> std::span<const core::UUID>;

	/**
	 * @brief
	 *  Counters.
	 * @return The counters.
	 */
	[[nodiscard]] auto getStats() const -> const Stats& { return m_stats; }

private:
	/// Ordering of cell coordinates, for the map.
	struct CoordsLess {
		/**
		 * @brief
		 *  Lexicographic order.
		 * @param[in] iA First coordinates.
		 * @param[in] iB Second coordinates.
		 * @return True when `iA` sorts first.
		 */
		auto operator()(const math::vec3i& iA, const math::vec3i& iB) const -> bool;
	};
	/// Runtime state of a cell.
	struct CellRuntime {
		/// The state.
		WorldCellState state = WorldCellState::Unloaded;
		/// Scene the load reads (a changed scene reloads the cell).
		std::string scene;
		/// Load ticket: a finished parse with another ticket is stale.
		uint64_t ticket = 0;
		/// Entities the cell brought.
		std::vector<core::UUID> entities;
	};
	/// A parse finished by a worker.
	struct FinishedLoad;
	/// Where the workers put their parses.
	struct Inbox;

	/// Start the load of a cell.
	void startLoad(const WorldCell& iCell, CellRuntime& ioRuntime, core::task::Scheduler* ioScheduler);
	/// Merge the finished loads, within the budget.
	void mergeFinished(Scene& ioScene, const WorldPartitionSettings& iSettings);
	/// Remove the entities of a cell.
	static void unloadCell(Scene& ioScene, CellRuntime& ioRuntime);
	/// Refresh the counters of the current states.
	void countStates();

	/// Runtime of the cells met, by coordinates.
	std::map<math::vec3i, CellRuntime, CoordsLess> m_cells;
	/// Parses finished by the workers.
	shared<Inbox> m_inbox;
	/// Next load ticket.
	uint64_t m_nextTicket = 1;
	/// Counters.
	Stats m_stats;
};

}// namespace owl::scene
