/**
 * @file ScenarioWatchdog.h
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace owl::nest::runner {

/**
 * @brief
 *  Stall detector of a scenario run: the runner reports each frame and the start of its shutdown, and a run that makes
 *  no progress for longer than the limit is described (phase, step, frame) instead of hanging until the CTest timeout.
 */
class ScenarioWatchdog final {
public:
	/// Clock of the watchdog.
	using clock = std::chrono::steady_clock;

	/// Phase of the run.
	enum struct Phase : uint8_t {
		Starting,///< Loading the scene, before the first frame.
		Playing,///< Playing the steps.
		ShuttingDown///< The verdict is out, the application is closing.
	};

	ScenarioWatchdog(const ScenarioWatchdog&) = delete;

	ScenarioWatchdog(ScenarioWatchdog&&) = delete;

	auto operator=(const ScenarioWatchdog&) -> ScenarioWatchdog& = delete;

	auto operator=(ScenarioWatchdog&&) -> ScenarioWatchdog& = delete;

	/**
	 * @brief
	 *  Create a watchdog.
	 * @param[in] iLimit The longest time without progress.
	 * @param[in] iStepCount The number of steps of the scenario.
	 * @param[in] iStart The time the run started.
	 */
	ScenarioWatchdog(clock::duration iLimit, size_t iStepCount, clock::time_point iStart);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ScenarioWatchdog() = default;

	/**
	 * @brief
	 *  Record a frame about to be played.
	 * @param[in] iStep The index of the current step.
	 */
	void onFrame(size_t iStep) noexcept;

	/**
	 * @brief
	 *  Record the start of the shutdown, once the verdict is out.
	 */
	void onShutdown() noexcept;

	/**
	 * @brief
	 *  Check the progress, from the watching thread.
	 * @param[in] iNow The current time.
	 * @return The description of the stall when nothing progressed for longer than the limit, nothing otherwise.
	 */
	[[nodiscard]] auto check(clock::time_point iNow) -> std::optional<std::string>;

	/**
	 * @brief
	 *  Get the limit of a run: 120 s, multiplied by a whole factor (`OWL_SCRIPT_TIME_SCALE`, slow emulated runs).
	 * @param[in] iScale The factor as text, or null for none.
	 * @return The limit.
	 */
	[[nodiscard]] static auto limitFor(const char* iScale) -> clock::duration;

	/**
	 * @brief
	 *  Start the process-wide watchdog thread: on a stall it prints the description to `stderr` and ends the process
	 *  with code 7. Its state is never destroyed, so it can watch the shutdown down to the process exit.
	 * @param[in] iStepCount The number of steps of the scenario.
	 */
	static void start(size_t iStepCount);

	/**
	 * @brief
	 *  Get the process-wide watchdog.
	 * @return The watchdog, null before `start`.
	 */
	[[nodiscard]] static auto get() -> ScenarioWatchdog*;

private:
	/// Longest time without progress.
	clock::duration m_limit;
	/// Number of steps of the scenario.
	size_t m_stepCount;
	/// Progress counter, bumped by every report.
	std::atomic<uint64_t> m_progress{0};
	/// Frames reported.
	std::atomic<uint64_t> m_frame{0};
	/// Current step index.
	std::atomic<size_t> m_step{0};
	/// Current phase.
	std::atomic<Phase> m_phase{Phase::Starting};
	/// Progress counter seen by the last check.
	uint64_t m_seenProgress{0};
	/// Time of the last progress seen.
	clock::time_point m_lastProgress;
};

}// namespace owl::nest::runner
