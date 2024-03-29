/**
 * @file Timestep.h
 * @author Silmaen
 * @date 10/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::core {

/**
 * @brief Time Steps.
 */
class OWL_API Timestep {
public:
	/// The type of clock.
	using clock = std::chrono::steady_clock;
	/// The type of time point.
	using time_point = clock::time_point;
	/// The tipe of duration.
	using duration = clock::duration;

	/**
	 * @brief Default constructor.
	 */
	Timestep() : m_lastCall{clock::now()} {
		m_statFps.resize(c_maxIndex, 0.0);
		update();
	}

	/**
	 * @brief Time step update.
	 */
	void update() {
		const time_point tp = clock::now();
		m_delta = tp - m_lastCall;
		m_lastCall = tp;
		if (m_delta.count() > 0) {
			m_statFps[m_index] = getFps();
			m_index = (m_index + 1) % c_maxIndex;
		}
	}

	/**
	 * @brief Get the seconds elapsed since last update.
	 * @return Seconds elapsed.
	 */
	[[nodiscard]] float getSeconds() const {
		return static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(m_delta).count()) / 1000.0f;
	}

	/**
	 * @brief Get the milliseconds elapsed since last update.
	 * @return Milliseconds elapsed.
	 */
	[[nodiscard]] float getMilliseconds() const {
		return static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(m_delta).count()) / 1000.0f;
	}

	/**
	 * @brief Get the mean number of update call in one second.
	 * @return The Frame per second number.
	 */
	[[nodiscard]] float getFps() const { return 1e3f / getMilliseconds(); }

	/**
	 * @brief Get the mean number of update call in one second.
	 * @return The Frame per second number.
	 */
	[[nodiscard]] float getStabilizedFps() const;

private:
	/// Last update call point.
	time_point m_lastCall{};
	/// The delta with the previous update call.
	duration m_delta{};
	/// Stabilized fps counters.
	std::vector<float> m_statFps;
	/// index in the stats.
	size_t m_index = 0;
	/// Max va in stats.
	static constexpr size_t c_maxIndex = 20;
};

}// namespace owl::core
