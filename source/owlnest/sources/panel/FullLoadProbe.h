/**
 * @file FullLoadProbe.h
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace owl::nest::panel {

/**
 * @brief
 *  Measures the frame rate of the editor running flat out, for a few seconds.
 *
 * Under vertical sync the CPU and the GPU lower their clocks, so the cost of a capped frame overstates what the
 * machine can do. While the probe runs the editor lifts every cap (no vsync, no limit, no idle wait); the first
 * second lets the clocks rise, the next ones are measured.
 */
class FullLoadProbe final {
public:
	/// The clock of the frame times.
	using clock = std::chrono::steady_clock;

	/// Time given to the clocks to rise before the measure.
	static constexpr clock::duration warmup = std::chrono::seconds(1);
	/// Duration of the measure.
	static constexpr clock::duration measure = std::chrono::seconds(2);

	/// Outcome of a probe.
	struct Result {
		/// Frames per second over the measure.
		double fps{0.0};
		/// Average CPU work of a frame, in milliseconds.
		double workMs{0.0};
		/// Average GPU busy time of a frame, in milliseconds, empty when no GPU timing arrived.
		std::optional<double> gpuMs;
	};

	/**
	 * @brief
	 *  Start a probe; a running one starts over.
	 * @param[in] iNow The current time.
	 */
	void start(clock::time_point iNow);

	/**
	 * @brief
	 *  Check if a probe runs: the caps must stay lifted.
	 * @return True between start() and the end of the measure.
	 */
	[[nodiscard]] auto isRunning() const -> bool { return m_running; }

	/**
	 * @brief
	 *  Record a frame of the running probe.
	 * @param[in] iWorkMs CPU time of the frame without the waits for the display, in milliseconds.
	 * @param[in] iNow The time the frame ended.
	 * @return True when this frame ended the measure (the result is ready, the caps can come back).
	 */
	auto addFrame(double iWorkMs, clock::time_point iNow) -> bool;

	/**
	 * @brief
	 *  Record the GPU busy time of a frame of the running probe.
	 * @param[in] iBusyMs GPU busy time, in milliseconds.
	 */
	void addGpu(double iBusyMs);

	/**
	 * @brief
	 *  Get the outcome of the last completed probe.
	 * @return The result, empty before the first probe ends.
	 */
	[[nodiscard]] auto getResult() const -> const std::optional<Result>& { return m_result; }

private:
	/// True while a probe runs.
	bool m_running{false};
	/// When the probe started.
	clock::time_point m_start;
	/// Frames recorded in the measure.
	uint32_t m_frames{0};
	/// Sum of the CPU work of the measured frames, in milliseconds.
	double m_workSum{0.0};
	/// GPU timings recorded in the measure.
	uint32_t m_gpuFrames{0};
	/// Sum of the measured GPU busy times, in milliseconds.
	double m_gpuSum{0.0};
	/// True once the warm-up is over.
	bool m_measuring{false};
	/// Outcome of the last completed probe.
	std::optional<Result> m_result;
};

}// namespace owl::nest::panel
