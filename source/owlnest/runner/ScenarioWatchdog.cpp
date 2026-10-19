/**
 * @file ScenarioWatchdog.cpp
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "ScenarioWatchdog.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string_view>
#include <system_error>
#include <thread>

namespace owl::nest::runner {

namespace {

constexpr int g_StallExitCode = 7;

auto phaseName(const ScenarioWatchdog::Phase iPhase) -> std::string_view {
	switch (iPhase) {
		case ScenarioWatchdog::Phase::Starting:
			return "starting (scene load, before the first frame)";
		case ScenarioWatchdog::Phase::Playing:
			return "playing";
		case ScenarioWatchdog::Phase::ShuttingDown:
			return "shutting down (verdict printed above)";
	}
	return "unknown";
}

std::atomic<ScenarioWatchdog*> g_instance{nullptr};

}// namespace

ScenarioWatchdog::ScenarioWatchdog(const clock::duration iLimit, const size_t iStepCount,
								   const clock::time_point iStart)
	: m_limit{iLimit}, m_stepCount{iStepCount}, m_lastProgress{iStart} {}

void ScenarioWatchdog::onFrame(const size_t iStep) noexcept {
	m_step.store(iStep, std::memory_order_relaxed);
	m_frame.fetch_add(1, std::memory_order_relaxed);
	m_phase.store(Phase::Playing, std::memory_order_relaxed);
	m_progress.fetch_add(1, std::memory_order_release);
}

void ScenarioWatchdog::onShutdown() noexcept {
	m_phase.store(Phase::ShuttingDown, std::memory_order_relaxed);
	m_progress.fetch_add(1, std::memory_order_release);
}

auto ScenarioWatchdog::check(const clock::time_point iNow) -> std::optional<std::string> {
	if (const uint64_t progress = m_progress.load(std::memory_order_acquire); progress != m_seenProgress) {
		m_seenProgress = progress;
		m_lastProgress = iNow;
		return std::nullopt;
	}
	const auto stalled = iNow - m_lastProgress;
	if (stalled <= m_limit)
		return std::nullopt;
	const auto phase = m_phase.load(std::memory_order_relaxed);
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(stalled).count();
	std::string message =
			std::format("Scenario watchdog: no progress for {} s, stuck while {}", seconds, phaseName(phase));
	if (phase == Phase::Playing)
		message += std::format(", frame {}, step {}/{}", m_frame.load(std::memory_order_relaxed),
							   std::min(m_step.load(std::memory_order_relaxed) + 1, m_stepCount), m_stepCount);
	message += ".";
	return message;
}

auto ScenarioWatchdog::limitFor(const char* iScale) -> clock::duration {
	constexpr auto baseLimit = std::chrono::seconds{120};
	if (iScale == nullptr)
		return baseLimit;
	uint32_t factor = 1;
	const std::string_view text{iScale};
	if (const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), factor);
		error != std::errc{} || factor == 0)
		return baseLimit;
	return baseLimit * factor;
}

void ScenarioWatchdog::start(const size_t iStepCount) {
	if (g_instance.load() != nullptr)
		return;
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory) never destroyed: it watches the process exit too.
	auto* watchdog = new ScenarioWatchdog(limitFor(std::getenv("OWL_SCRIPT_TIME_SCALE")), iStepCount, clock::now());
	g_instance.store(watchdog);
	std::thread([watchdog]() -> void {
		const auto tick = std::min<clock::duration>(watchdog->m_limit / 4, std::chrono::seconds{1});
		while (true) {
			std::this_thread::sleep_for(tick);
			if (const auto stall = watchdog->check(clock::now()); stall.has_value()) {
				// Not std::println: on MinGW its console path lives in libstdc++exp, which nothing links.
				std::fputs(std::format("{}\n", *stall).c_str(), stderr);
				std::fflush(stderr);
				std::_Exit(g_StallExitCode);
			}
		}
	}).detach();
}

auto ScenarioWatchdog::get() -> ScenarioWatchdog* { return g_instance.load(); }

}// namespace owl::nest::runner
