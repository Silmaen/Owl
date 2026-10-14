/**
 * @file SimulationClock_test.cpp
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <scene/Scene.h>
#include <scene/SimulationClock.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <tuple>

using namespace owl;
using namespace owl::scene;

namespace {

auto makeFrame(const int64_t iMicroseconds) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(iMicroseconds));
	return ts;
}

}// namespace

TEST(SimulationClock, DefaultsToSixtyHertz) {
	const SimulationClock clock;
	EXPECT_FLOAT_EQ(clock.getSettings().tickRate, 60.f);
	EXPECT_EQ(clock.getSettings().maxTicksPerFrame, 8u);
	EXPECT_NEAR(clock.getStepSeconds(), 1.0 / 60.0, 1e-12);
	EXPECT_NEAR(clock.getStepTimestep().getSeconds(), 1.f / 60.f, 1e-6f);
	EXPECT_EQ(clock.getTotalTickCount(), 0u);
}

TEST(SimulationClock, TicksFollowTheAccumulatedTime) {
	SimulationClock clock;
	clock.setSettings({.tickRate = 100.f});
	EXPECT_EQ(clock.advance(makeFrame(4'000)), 0u);
	EXPECT_NEAR(clock.getAlpha(), 0.4f, 1e-4f);
	EXPECT_EQ(clock.advance(makeFrame(7'000)), 1u);
	EXPECT_NEAR(clock.getAlpha(), 0.1f, 1e-4f);
	EXPECT_EQ(clock.advance(makeFrame(29'000)), 3u);
	EXPECT_EQ(clock.getFrameTickCount(), 3u);
	EXPECT_NEAR(clock.getAlpha(), 0.f, 1e-4f);
	EXPECT_EQ(clock.getTotalTickCount(), 4u);
}

TEST(SimulationClock, FramesOfOneStepGiveOneTickEach) {
	SimulationClock clock;
	// 1/60 s truncated to whole microseconds: the tolerance keeps one tick per frame.
	for (int i = 0; i < 600; ++i) ASSERT_EQ(clock.advance(makeFrame(16'666)), 1u) << "frame " << i;
	EXPECT_EQ(clock.getTotalTickCount(), 600u);
}

TEST(SimulationClock, TicksPerFrameAreBoundedAndTheExcessIsDropped) {
	SimulationClock clock;
	clock.setSettings({.maxTicksPerFrame = 5});
	EXPECT_EQ(clock.advance(makeFrame(2'000'000)), 5u);
	EXPECT_LT(clock.getAlpha(), 1.f);
	EXPECT_EQ(clock.advance(makeFrame(0)), 0u);
	EXPECT_EQ(clock.advance(makeFrame(16'667)), 1u);
}

TEST(SimulationClock, NegativeFramesCountAsZero) {
	SimulationClock clock;
	core::Timestep ts;
	ts.forceUpdate(-std::chrono::milliseconds(50));
	EXPECT_EQ(clock.advance(ts), 0u);
	EXPECT_FLOAT_EQ(clock.getAlpha(), 0.f);
}

TEST(SimulationClock, ResetDropsTheAccumulatedTime) {
	SimulationClock clock;
	std::ignore = clock.advance(makeFrame(30'000));
	EXPECT_EQ(clock.getTotalTickCount(), 1u);
	clock.reset();
	EXPECT_EQ(clock.getTotalTickCount(), 0u);
	EXPECT_FLOAT_EQ(clock.getAlpha(), 0.f);
	EXPECT_EQ(clock.advance(makeFrame(10'000)), 0u);
}

TEST(SimulationSettings, ClampedKeepsFieldsInRange) {
	SimulationSettings settings{.tickRate = 0.f, .maxTicksPerFrame = 0};
	auto clamped = settings.clamped();
	EXPECT_FLOAT_EQ(clamped.tickRate, float{SimulationSettings::minTickRate});
	EXPECT_EQ(clamped.maxTicksPerFrame, 1u);
	settings = {.tickRate = 5000.f, .maxTicksPerFrame = 1000};
	clamped = settings.clamped();
	EXPECT_FLOAT_EQ(clamped.tickRate, float{SimulationSettings::maxTickRate});
	EXPECT_EQ(clamped.maxTicksPerFrame, uint32_t{SimulationSettings::maxTicksLimit});
	settings.tickRate = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FLOAT_EQ(settings.clamped().tickRate, float{SimulationSettings::defaultTickRate});
	EXPECT_NEAR(SimulationSettings{.tickRate = 0.f}.getStepSeconds(), 1.0, 1e-12);
	SimulationClock clock;
	clock.setSettings({.tickRate = -3.f, .maxTicksPerFrame = 0});
	EXPECT_FLOAT_EQ(clock.getSettings().tickRate, float{SimulationSettings::minTickRate});
}

TEST(SimulationClock, SceneTakesTheTickRateOfItsEngineContext) {
	core::Log::init(core::Log::Level::Off);
	app::EngineContext context;
	context.setSimulationSettings({.tickRate = 20.f, .maxTicksPerFrame = 2});
	Scene scene;
	scene.setEngineContext(&context);
	scene.getSimulationClock().setSettings({.tickRate = 90.f});
	std::ignore = scene.getSimulationClock().advance(makeFrame(5'000));
	scene.onStartRuntime();
	EXPECT_FLOAT_EQ(scene.getSimulationClock().getSettings().tickRate, 20.f);
	EXPECT_FLOAT_EQ(scene.getSimulationClock().getAlpha(), 0.f);
	scene.onUpdateRuntime(makeFrame(500'000), false);
	EXPECT_EQ(scene.getSimulationClock().getFrameTickCount(), 2u);
	scene.onEndRuntime();
	const auto source = mkShared<Scene>();
	source->getSimulationClock().setSettings({.tickRate = 45.f});
	EXPECT_FLOAT_EQ(Scene::copy(source)->getSimulationClock().getSettings().tickRate, 45.f);
	core::Log::invalidate();
}
