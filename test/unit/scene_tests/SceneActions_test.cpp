/**
 * @file SceneActions_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <input/ActionMap.h>
#include <input/Input.h>
#include <scene/Scene.h>
#include <scene/SystemSchedule.h>
#include <window/Window.h>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace owl;
using namespace owl::scene;

namespace {

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(iMs));
	return step;
}

/// Presses seen by a tick system and by a frame system.
struct PressCounts {
	int tick = 0;
	int frame = 0;
};

class SceneActionsTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		input::Input::init(window::Type::Null);
		scene.getSystems() = SystemSchedule{};
		scene.getSimulationClock().setSettings({.tickRate = 100.f});
		ASSERT_TRUE(scene.getSystems().add({.name = "game.tick",
											.phase = SystemPhase::Scripts,
											.tick = [this](const Scene& iScene, const SystemContext&) -> void {
												counts.tick += iScene.getActions().isJustPressed("jump") ? 1 : 0;
											}}));
		ASSERT_TRUE(scene.getSystems().add({.name = "game.frame",
											.phase = SystemPhase::Late,
											.update = [this](const Scene& iScene, const SystemContext&) -> void {
												counts.frame += iScene.getActions().isJustPressed("jump") ? 1 : 0;
											}}));
	}

	void TearDown() override {
		scene.onEndRuntime();
		input::Input::invalidate();
		core::Log::invalidate();
	}

	Scene scene;
	PressCounts counts;
};

}// namespace

TEST_F(SceneActionsTest, BuiltInActionsFollowTheKeys) {
	scene.onStartRuntime();
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_FALSE(scene.getActions().isHeld("jump"));
	input::Input::injectKey(input::key::Space);
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_TRUE(scene.getActions().isHeld("jump"));
	EXPECT_EQ(counts.frame, 1);
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_EQ(counts.frame, 1);
	EXPECT_TRUE(scene.getActions().isHeld("jump"));
}

TEST_F(SceneActionsTest, SeveralTicksPerFrameSeeOneEdge) {
	scene.onStartRuntime();
	input::Input::injectKey(input::key::Space);
	scene.onUpdateRuntime(makeStep(35), false);// three ticks
	EXPECT_EQ(counts.tick, 1);
	EXPECT_EQ(counts.frame, 1);
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_EQ(counts.tick, 1);
}

TEST_F(SceneActionsTest, TheStartHeldStateIsNotAnEdge) {
	input::Input::injectKey(input::key::Space);
	scene.onStartRuntime();
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_EQ(counts.tick, 0);
	EXPECT_EQ(counts.frame, 0);
	EXPECT_TRUE(scene.getActions().isHeld("jump"));
}

TEST_F(SceneActionsTest, TheContextMapRebindsAnAction) {
	app::EngineContext context;
	const auto parsed = input::ActionMap::fromYaml("actions:\n  - {name: jump, bindings: [{key: F}]}\n");
	ASSERT_TRUE(parsed.has_value());
	context.setActionMap(*parsed);
	EXPECT_NE(context.getActionMap()->find("interact"), nullptr);// completed with the built-in actions
	scene.setEngineContext(&context);
	scene.onStartRuntime();
	input::Input::injectKey(input::key::Space);
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_FALSE(scene.getActions().isHeld("jump"));
	input::Input::injectKey(input::key::F);
	scene.onUpdateRuntime(makeStep(35), false);
	EXPECT_TRUE(scene.getActions().isHeld("jump"));
	EXPECT_EQ(counts.frame, 1);
	scene.setEngineContext(nullptr);
}

TEST(EngineContextActions, LoadsTheProjectMapOrFallsBack) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_engine_context_actions";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	app::EngineContext context;
	EXPECT_FALSE(context.loadActionMap(dir));
	EXPECT_EQ(context.getActionMap()->size(), input::ActionMap::createDefault().size());
	EXPECT_FALSE(context.loadActionMap({}));
	std::ofstream(dir / "input_actions.yml") << "actions:\n  - {name: fire, bindings: [{key: F}]}\n";
	EXPECT_TRUE(context.loadActionMap(dir));
	EXPECT_NE(context.getActionMap()->find("fire"), nullptr);
	EXPECT_NE(context.getActionMap()->find("jump"), nullptr);
	std::ofstream(dir / "input_actions.yml") << "actions:\n  - {name: fire, type: slider}\n";
	EXPECT_FALSE(context.loadActionMap(dir));
	EXPECT_EQ(context.getActionMap()->find("fire"), nullptr);
	EXPECT_TRUE(context.loadActionMapFromText("actions:\n  - {name: duck, bindings: [{key: C}]}\n"));
	EXPECT_NE(context.getActionMap()->find("duck"), nullptr);
	EXPECT_FALSE(context.loadActionMapFromText("not: [valid"));
	EXPECT_EQ(context.getActionMap()->find("duck"), nullptr);
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
