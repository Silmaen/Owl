/**
 * @file GpuCounterMonitor.h
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <renderer/gpu/RenderAPI.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace owl::nest::panel {

/**
 * @brief
 *  Per-frame deltas of the cumulative backend counters (`RenderCommand::getRenderCounters`).
 */
struct GpuCounterDeltas {
	/// Command buffer submissions.
	uint64_t submits{0};
	/// `vkQueueWaitIdle` calls.
	uint64_t queueWaitIdles{0};
	/// `vkDeviceWaitIdle` calls.
	uint64_t deviceWaitIdles{0};
	/// Blocking fence waits besides the frame pacing.
	uint64_t fenceWaits{0};
	/// CPU time blocked by the frame pacing, in milliseconds.
	double paceWaitMs{0.0};
};

/**
 * @brief
 *  Turns the cumulative GPU counters into per-frame deltas and keeps their maximum over a sliding window.
 *
 * The window is split into a fixed number of time buckets, each holding the maximum of its frames: a one-off drain
 * stays visible for the whole window and nothing is allocated per frame.
 */
class GpuCounterMonitor final {
public:
	/// The clock of the frame times.
	using clock = std::chrono::steady_clock;

	/// Number of buckets in the sliding window.
	static constexpr size_t bucketCount = 8;
	/// Duration covered by one bucket (the window spans `bucketCount` of them, 2 s).
	static constexpr clock::duration bucketDuration = std::chrono::milliseconds(250);

	/**
	 * @brief
	 *  Record the counters read at the start of a frame.
	 *
	 * The first call only sets the baseline; a counter going backwards (backend restart) restarts the baseline too.
	 * @param[in] iCounters The cumulative counters.
	 * @param[in] iNow The time of the read.
	 */
	void update(const renderer::gpu::RenderCounters& iCounters, clock::time_point iNow);

	/**
	 * @brief
	 *  Forget every sample, the next update sets a new baseline.
	 */
	void reset();

	/**
	 * @brief
	 *  Deltas of the last frame.
	 * @return The deltas between the last two updates.
	 */
	[[nodiscard]] auto getLast() const -> const GpuCounterDeltas& { return m_last; }

	/**
	 * @brief
	 *  Maximum of each delta over the sliding window.
	 * @return The per-field maxima of the frames of the last `bucketCount * bucketDuration`.
	 */
	[[nodiscard]] auto getWindowMax() const -> GpuCounterDeltas;

private:
	/// Counters of the previous update.
	renderer::gpu::RenderCounters m_previous;
	/// True once a baseline was recorded.
	bool m_hasPrevious{false};
	/// Deltas of the last frame.
	GpuCounterDeltas m_last;
	/// Maximum of the deltas of each bucket.
	std::array<GpuCounterDeltas, bucketCount> m_buckets{};
	/// Absolute index of the current bucket (time since the epoch divided by the bucket duration).
	int64_t m_bucketIndex{-1};
};

}// namespace owl::nest::panel
