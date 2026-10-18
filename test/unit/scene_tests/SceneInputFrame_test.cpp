/**
 * @file SceneInputFrame_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <input/Input.h>
#include <scene/Scene.h>
#include <scene/SystemSchedule.h>
#include <window/Window.h>

#include <chrono>

using namespace owl;
using namespace owl::scene;

namespace {

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(iMs));
	return step;
}

/// Edges counted by a tick system and by a frame system of a test scene.
struct EdgeCounts {
	int tickPresses = 0;
	int tickReleases = 0;
	int framePresses = 0;
	int frameReleases = 0;
};

class SceneInputFrameTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		input::Input::init(window::Type::Null);
		scene.getSystems() = SystemSchedule{};
		scene.getSimulationClock().setSettings({.tickRate = 100.f});
		ASSERT_TRUE(scene.getSystems().add({.name = "game.tick",
											.phase = SystemPhase::Scripts,
											.tick = [this](const Scene& iScene, const SystemContext&) -> void {
												const auto& frame = iScene.getInputFrame();
												counts.tickPresses += frame.isKeyJustPressed(input::key::E) ? 1 : 0;
												counts.tickReleases += frame.isKeyJustReleased(input::key::E) ? 1 : 0;
											}}));
		ASSERT_TRUE(scene.getSystems().add({.name = "game.frame",
											.phase = SystemPhase::Late,
											.update = [this](const Scene& iScene, const SystemContext&) -> void {
												const auto& frame = iScene.getInputFrame();
												counts.framePresses += frame.isKeyJustPressed(input::key::E) ? 1 : 0;
												counts.frameReleases += frame.isKeyJustReleased(input::key::E) ? 1 : 0;
											}}));
	}

	void TearDown() override {
		scene.onEndRuntime();
		input::Input::invalidate();
		core::Log::invalidate();
	}

	Scene scene;
	EdgeCounts counts;
};

}// namespace

TEST_F(SceneInputFrameTest, SeveralTicksPerFrameSeeOneEdge) {
	scene.onStartRuntime();
	input::Input::injectKey(input::key::E);
	scene.onUpdateRuntime(makeStep(35), false);// three ticks
	EXPECT_EQ(counts.tickPresses, 1);
	EXPECT_EQ(counts.framePresses, 1);
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_EQ(counts.tickPresses, 1);
	EXPECT_EQ(counts.framePresses, 1);
	input::Input::injectKey(input::key::E);
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_EQ(counts.tickReleases, 1);
	EXPECT_EQ(counts.frameReleases, 1);
}

TEST_F(SceneInputFrameTest, FramesWithoutTickLoseNoEdge) {
	scene.onStartRuntime();
	// A press and a release in two frames shorter than a tick.
	input::Input::injectKey(input::key::E);
	scene.onUpdateRuntime(makeStep(2), false);
	input::Input::injectKey(input::key::E);
	scene.onUpdateRuntime(makeStep(2), false);
	EXPECT_EQ(counts.tickPresses, 0);
	EXPECT_EQ(counts.framePresses, 1);
	EXPECT_EQ(counts.frameReleases, 1);
	scene.onUpdateRuntime(makeStep(8), false);// first tick
	EXPECT_EQ(counts.tickPresses, 1);
	EXPECT_EQ(counts.tickReleases, 0);
	scene.onUpdateRuntime(makeStep(10), false);
	EXPECT_EQ(counts.tickPresses, 1);
	EXPECT_EQ(counts.tickReleases, 1);
	EXPECT_EQ(counts.framePresses, 1);
	EXPECT_EQ(counts.frameReleases, 1);
}

TEST_F(SceneInputFrameTest, KeyHeldAtStartGivesNoEdge) {
	input::Input::injectKey(input::key::E);
	scene.onStartRuntime();
	scene.onUpdateRuntime(makeStep(20), false);
	EXPECT_EQ(counts.tickPresses, 0);
	EXPECT_EQ(counts.framePresses, 0);
	EXPECT_TRUE(scene.getInputFrame().isKeyPressed(input::key::E));
}
