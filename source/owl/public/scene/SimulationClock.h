/**
 * @file SimulationClock.h
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Timestep.h"

#include <cstdint>

namespace owl::scene {

/**
 * @brief
 *  Rate of the fixed simulation tick, a project setting (`simulation:` in `owl_project.yml`).
 */
struct OWL_API SimulationSettings {
	/// Default number of ticks per second.
	static constexpr float defaultTickRate = 60.f;
	/// Lowest accepted tick rate, in ticks per second.
	static constexpr float minTickRate = 1.f;
	/// Highest accepted tick rate, in ticks per second.
	static constexpr float maxTickRate = 1000.f;
	/// Default bound of the ticks run by one frame.
	static constexpr uint32_t defaultMaxTicksPerFrame = 8;
	/// Highest accepted bound of the ticks run by one frame.
	static constexpr uint32_t maxTicksLimit = 64;

	/// Fixed ticks per second.
	float tickRate = defaultTickRate;
	/// Maximum number of ticks run by one frame; the time beyond is dropped (no spiral of death).
	uint32_t maxTicksPerFrame = defaultMaxTicksPerFrame;

	/**
	 * @brief
	 *  Duration of one tick.
	 * @return The tick duration in seconds, from the clamped tick rate.
	 */
	[[nodiscard]] auto getStepSeconds() const -> double;

	/**
	 * @brief
	 *  Copy of these settings with every field brought into its accepted range.
	 * @return The clamped settings (a non-finite tick rate becomes the default one).
	 */
	[[nodiscard]] auto clamped() const -> SimulationSettings;

	/**
	 * @brief
	 *  Field-wise comparison.
	 * @param[in] iOther The settings to compare with.
	 * @return True when all fields are equal.
	 */
	auto operator==(const SimulationSettings& iOther) const -> bool;
};

/**
 * @brief
 *  Accumulator turning variable frame durations into a whole number of fixed ticks.
 *
 * Each frame adds its duration to the accumulator and runs as many ticks of `1 / tickRate` seconds as it holds, at
 * most `maxTicksPerFrame` (the time beyond is dropped). The leftover time over the tick duration is the interpolation
 * alpha, to blend what is drawn between the last two ticks.
 */
class OWL_API SimulationClock final {
public:
	/**
	 * @brief
	 *  Create a clock with the default settings (60 Hz).
	 */
	SimulationClock();

	/**
	 * @brief
	 *  Destructor.
	 */
	~SimulationClock() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	SimulationClock(const SimulationClock&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	SimulationClock(SimulationClock&&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This clock.
	 */
	auto operator=(const SimulationClock&) -> SimulationClock& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This clock.
	 */
	auto operator=(SimulationClock&&) -> SimulationClock& = default;

	/**
	 * @brief
	 *  Change the tick rate and the tick bound; the accumulated time is kept.
	 * @param[in] iSettings The settings, clamped into their accepted range.
	 */
	void setSettings(const SimulationSettings& iSettings);

	/**
	 * @brief
	 *  Settings in use.
	 * @return The clamped settings.
	 */
	[[nodiscard]] auto getSettings() const -> const SimulationSettings& { return m_settings; }

	/**
	 * @brief
	 *  Drop the accumulated time and the tick counters (start of a run).
	 */
	void reset();

	/**
	 * @brief
	 *  Add a frame duration and compute the ticks it holds.
	 * @param[in] iFrame The duration of the frame (a negative one counts as zero).
	 * @return The number of ticks to run this frame, at most `maxTicksPerFrame`.
	 */
	auto advance(const core::Timestep& iFrame) -> uint32_t;

	/**
	 * @brief
	 *  Number of ticks computed by the last `advance()`.
	 * @return The tick count.
	 */
	[[nodiscard]] auto getFrameTickCount() const -> uint32_t { return m_frameTicks; }

	/**
	 * @brief
	 *  Number of ticks since the last `reset()`.
	 * @return The tick count.
	 */
	[[nodiscard]] auto getTotalTickCount() const -> uint64_t { return m_totalTicks; }

	/**
	 * @brief
	 *  Interpolation factor between the last two ticks.
	 * @return The leftover accumulated time over the tick duration, in [0, 1).
	 */
	[[nodiscard]] auto getAlpha() const -> float { return m_alpha; }

	/**
	 * @brief
	 *  Duration of one tick.
	 * @return The tick duration in seconds.
	 */
	[[nodiscard]] auto getStepSeconds() const -> double { return m_stepSeconds; }

	/**
	 * @brief
	 *  Duration of one tick, as the time step given to the tick callbacks.
	 * @return The tick time step.
	 */
	[[nodiscard]] auto getStepTimestep() const -> const core::Timestep& { return m_stepTimestep; }

private:
	/// Clamped settings.
	SimulationSettings m_settings;
	/// Duration of one tick, in seconds.
	double m_stepSeconds;
	/// Duration of one tick, built once for the tick callbacks.
	core::Timestep m_stepTimestep;
	/// Time not consumed by a tick yet, in seconds.
	double m_accumulator = 0.0;
	/// Ticks of the last `advance()`.
	uint32_t m_frameTicks = 0;
	/// Ticks since the last `reset()`.
	uint64_t m_totalTicks = 0;
	/// Leftover time over the tick duration after the last `advance()`.
	float m_alpha = 0.f;
};

}// namespace owl::scene
