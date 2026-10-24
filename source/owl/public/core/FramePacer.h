/**
 * @file FramePacer.h
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Core.h"

#include <chrono>
#include <cstdint>

namespace owl::core {

/**
 * @brief
 *  Spaces loop iterations by a fixed period: each call schedules the next deadline one period after the previous one.
 *
 * A late iteration restarts the schedule from now instead of letting the next ones run back to back to catch up.
 */
class OWL_API FramePacer {
public:
	/// The clock of the deadlines.
	using clock = std::chrono::steady_clock;

	/**
	 * @brief
	 *  Set the period between two iterations and restart the schedule.
	 * @param[in] iPeriod The period, zero or negative for no pacing.
	 * @param[in] iNow The current time.
	 */
	void setPeriod(clock::duration iPeriod, clock::time_point iNow);

	/**
	 * @brief
	 *  Set the period from a number of iterations per second and restart the schedule.
	 * @param[in] iPerSecond Iterations per second, 0 for no pacing.
	 * @param[in] iNow The current time.
	 */
	void setRate(uint32_t iPerSecond, clock::time_point iNow);

	/**
	 * @brief
	 *  Get the period between two iterations.
	 * @return The period, zero without pacing.
	 */
	[[nodiscard]] auto getPeriod() const -> clock::duration { return m_period; }

	/**
	 * @brief
	 *  Check if the iterations are paced.
	 * @return True when a period is set.
	 */
	[[nodiscard]] auto isActive() const -> bool { return m_period > clock::duration::zero(); }

	/**
	 * @brief
	 *  Schedule the next iteration.
	 * @param[in] iNow The end of the current iteration.
	 * @return When the next iteration may start: @p iNow when it is late or without pacing.
	 */
	auto next(clock::time_point iNow) -> clock::time_point;

	/**
	 * @brief
	 *  Schedule the next iteration and sleep until it may start.
	 */
	void wait();

private:
	/// Period between two iterations, zero for none.
	clock::duration m_period{};
	/// Deadline of the next iteration.
	clock::time_point m_deadline;
};

}// namespace owl::core
