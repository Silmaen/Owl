/**
 * @file FrameMeter.h
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
 *  Averages the frame costs over short windows so the editor can show what a frame costs, not how often the display
 *  takes one.
 *
 * Under vertical sync the interval between two frames is the refresh period whatever the frame costs. The meter
 * averages the CPU work of the frames (the waits for the display excluded) and their GPU busy time, and derives the
 * frame rate they would allow: `1000 / max(CPU, GPU)`, CPU and GPU working in parallel.
 */
class FrameMeter final {
public:
	/// The clock of the frame times.
	using clock = std::chrono::steady_clock;

	/// Duration of one averaging window; the published values change once per window.
	static constexpr clock::duration window = std::chrono::milliseconds(500);

	/**
	 * @brief
	 *  Record the CPU work of a frame.
	 * @param[in] iWorkMs CPU time of the frame without the waits for the display, in milliseconds.
	 * @param[in] iNow The time the frame ended.
	 */
	void addFrame(double iWorkMs, clock::time_point iNow);

	/**
	 * @brief
	 *  Record the GPU busy time of a frame (GPU timings arrive a few frames late).
	 * @param[in] iBusyMs GPU busy time of the frame, in milliseconds.
	 */
	void addGpu(double iBusyMs);

	/**
	 * @brief
	 *  Forget every sample and every published value.
	 */
	void reset();

	/**
	 * @brief
	 *  Average CPU work of the frames of the last window.
	 * @return The work in milliseconds, 0 before the first window ends.
	 */
	[[nodiscard]] auto getWorkMs() const -> double { return m_workMs; }

	/**
	 * @brief
	 *  Average GPU busy time of the frames of the last window.
	 * @return The busy time in milliseconds, empty when no GPU timing arrived.
	 */
	[[nodiscard]] auto getGpuMs() const -> std::optional<double> { return m_gpuMs; }

	/**
	 * @brief
	 *  Frames presented per second over the last window (the display rate under vertical sync).
	 * @return The frame rate, 0 before the first window ends.
	 */
	[[nodiscard]] auto getDisplayFps() const -> double { return m_displayFps; }

	/**
	 * @brief
	 *  Frame rate the frame costs would allow without any display pacing.
	 * @return `1000 / max(CPU, GPU)`, 0 before the first window ends.
	 */
	[[nodiscard]] auto getPotentialFps() const -> double;

private:
	/// Start of the current window.
	clock::time_point m_windowStart;
	/// True once the first frame opened a window.
	bool m_started{false};
	/// Frames recorded in the current window.
	uint32_t m_frames{0};
	/// Sum of the CPU work of the current window, in milliseconds.
	double m_workSum{0.0};
	/// GPU timings recorded in the current window.
	uint32_t m_gpuFrames{0};
	/// Sum of the GPU busy times of the current window, in milliseconds.
	double m_gpuSum{0.0};
	/// Published average CPU work.
	double m_workMs{0.0};
	/// Published average GPU busy time.
	std::optional<double> m_gpuMs;
	/// Published presented frame rate.
	double m_displayFps{0.0};
};

}// namespace owl::nest::panel
