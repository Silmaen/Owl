/**
 * @file SystemSchedule_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SystemSchedule.h>
#include <scene/component/PhysicBody.h>
#include <scene/component/Transform.h>

#include <chrono>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

// The name is a literal: a capture that fits std::function's inline buffer keeps the static analyzer from losing the
// heap copy a captured std::string would need (false NewDeleteLeaks).
auto recorder(std::vector<std::string>& ioLog, const char* iName) -> SystemFunction {
	return [&ioLog, iName](Scene&, const SystemContext&) -> void { ioLog.emplace_back(iName); };
}

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(iMs));
	return step;
}

}// namespace

TEST(SystemSchedule, RunsPhaseByPhaseInInsertionOrder) {
	core::Log::init(core::Log::Level::Off);
	std::vector<std::string> log;
	SystemSchedule schedule;
	EXPECT_TRUE(schedule.add({.name = "late", .phase = SystemPhase::Late, .update = recorder(log, "late")}));
	EXPECT_TRUE(schedule.add({.name = "a", .phase = SystemPhase::Scripts, .update = recorder(log, "a")}));
	EXPECT_TRUE(schedule.add({.name = "b", .phase = SystemPhase::Scripts, .update = recorder(log, "b")}));
	EXPECT_TRUE(schedule.insertBefore(
			"a", {.name = "first", .phase = SystemPhase::PrePhysics, .update = recorder(log, "first")}));
	EXPECT_EQ(schedule.getNames(SystemPhase::Scripts), (std::vector<std::string>{"first", "a", "b"}));
	EXPECT_EQ(schedule.getNames(SystemPhase::Late), std::vector<std::string>{"late"});
	Scene scene;
	schedule.run(SystemPhase::Scripts, scene, {});
	EXPECT_EQ(log, (std::vector<std::string>{"first", "a", "b"}));
	core::Log::invalidate();
}

TEST(SystemSchedule, RejectsInvalidEdits) {
	core::Log::init(core::Log::Level::Off);
	std::vector<std::string> log;
	SystemSchedule schedule;
	EXPECT_FALSE(schedule.add({.name = "", .phase = SystemPhase::PrePhysics, .update = recorder(log, "x")}));
	EXPECT_FALSE(schedule.add({.name = "noop", .phase = SystemPhase::PrePhysics, .update = nullptr}));
	EXPECT_TRUE(schedule.add({.name = "x", .phase = SystemPhase::PrePhysics, .update = recorder(log, "x")}));
	EXPECT_FALSE(schedule.add({.name = "x", .phase = SystemPhase::PrePhysics, .update = recorder(log, "x")}));
	EXPECT_FALSE(schedule.insertBefore("missing",
									   {.name = "y", .phase = SystemPhase::PrePhysics, .update = recorder(log, "y")}));
	EXPECT_FALSE(schedule.replace("missing", recorder(log, "z")));
	EXPECT_FALSE(schedule.replace("x", nullptr));
	EXPECT_FALSE(schedule.remove("missing"));
	EXPECT_TRUE(schedule.replace("x", recorder(log, "replaced")));
	Scene scene;
	schedule.run(SystemPhase::PrePhysics, scene, {});
	EXPECT_EQ(log, std::vector<std::string>{"replaced"});
	EXPECT_TRUE(schedule.remove("x"));
	EXPECT_FALSE(schedule.has("x"));
	core::Log::invalidate();
}

TEST(SystemSchedule, NewScenesCopyTheEngineSystems) {
	const auto engine = SystemSchedule::makeEngineDefault();
	for (const char* name:
		 {"owl.scripts", "owl.fly_cameras", "owl.voxel_players", "owl.raycast_walls", "owl.player_input", "owl.physics",
		  "owl.entity_links", "owl.triggers", "owl.sound", "owl.sprite_animation", "owl.game_over"})
		EXPECT_TRUE(engine.has(name)) << name;
	EXPECT_EQ(engine.getNames(SystemPhase::Physics), std::vector<std::string>{"owl.physics"});
	const Scene scene;
	EXPECT_EQ(scene.getSystems().getNames(SystemPhase::PrePhysics), engine.getNames(SystemPhase::PrePhysics));
}

TEST(SystemSchedule, SceneRunsItsOwnSystems) {
	core::Log::init(core::Log::Level::Off);
	std::vector<std::string> log;
	const auto scene = mkShared<Scene>();
	ASSERT_TRUE(scene->getSystems().add(
			{.name = "game.pre", .phase = SystemPhase::PrePhysics, .update = recorder(log, "pre")}));
	ASSERT_TRUE(scene->getSystems().add(
			{.name = "game.late", .phase = SystemPhase::Late, .update = recorder(log, "late")}));
	ASSERT_TRUE(scene->getSystems().insertBefore(
			"owl.physics", {.name = "game.before_physics", .update = recorder(log, "before_physics")}));
	scene->onStartRuntime();
	scene->onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(log, (std::vector<std::string>{"pre", "before_physics", "late"}));
	// A copy (Play) keeps the scene's systems; a fresh scene does not get them.
	const auto copy = Scene::copy(scene);
	EXPECT_TRUE(copy->getSystems().has("game.late"));
	EXPECT_FALSE(Scene{}.getSystems().has("game.late"));
	scene->onEndRuntime();
	core::Log::invalidate();
}

TEST(SystemSchedule, RemovingTheEnginePhysicsStopsTheBodies) {
	core::Log::init(core::Log::Level::Off);
	Scene scene;
	auto box = scene.createEntity("box");
	box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
	ASSERT_TRUE(scene.getSystems().remove("owl.physics"));
	scene.onStartRuntime();
	for (int i = 0; i < 10; ++i) scene.onUpdateRuntime(makeStep(16), false);
	EXPECT_FLOAT_EQ(box.getComponent<component::Transform>().transform.translation().y(), 0.f);
	scene.onEndRuntime();
	core::Log::invalidate();
}

TEST(SystemSchedule, GameOverRunsOnlyTheEndedPhase) {
	core::Log::init(core::Log::Level::Off);
	std::vector<std::string> log;
	Scene scene;
	ASSERT_TRUE(scene.getSystems().replace("owl.game_over", recorder(log, "ended")));
	ASSERT_TRUE(scene.getSystems().add(
			{.name = "game.pre", .phase = SystemPhase::PrePhysics, .update = recorder(log, "pre")}));
	scene.onStartRuntime();
	scene.status = Scene::Status::Victory;
	scene.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(log, std::vector<std::string>{"ended"});
	scene.onEndRuntime();
	core::Log::invalidate();
}
