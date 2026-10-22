/**
 * @file BehaviourComponents_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

using namespace owl;
using namespace owl::scene;

namespace {

class BehaviourTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames, const int iMilliseconds = 10) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(iMilliseconds));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto translationOf(const Entity& iEntity) -> math::vec3 {
		return iEntity.getComponent<component::Transform>().transform.translation();
	}
};

}// namespace

TEST_F(BehaviourTest, OscillatorShareFollowsTheCycle) {
	component::Oscillator oscillator;
	oscillator.period = 4.f;
	EXPECT_NEAR(oscillator.share(0.f), 0.f, 1e-5f);
	EXPECT_NEAR(oscillator.share(1.f), 0.5f, 1e-5f);
	EXPECT_NEAR(oscillator.share(2.f), 1.f, 1e-5f);
	EXPECT_NEAR(oscillator.share(4.f), 0.f, 1e-5f);
	oscillator.easing = component::Oscillator::Easing::Smooth;
	EXPECT_NEAR(oscillator.share(1.f), 0.5f, 1e-5f);
	EXPECT_LT(oscillator.share(0.2f), 0.1f);
	oscillator.phase = 0.5f;
	EXPECT_NEAR(oscillator.share(0.f), 1.f, 1e-5f);
}

TEST_F(BehaviourTest, AnOscillatorMovesATransformBetweenItsRestAndItsOffset) {
	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Lamp");
	entity.getComponent<component::Transform>().transform.translation() = {1.f, 2.f, 0.f};
	auto& oscillator = entity.addComponent<component::Oscillator>();
	oscillator.offset = {0.f, 3.f, 0.f};
	oscillator.period = 2.f;
	scene->onStartRuntime();
	run(*scene, 100);
	EXPECT_NEAR(translationOf(entity).y(), 5.f, 0.05f);
	EXPECT_NEAR(translationOf(entity).x(), 1.f, 1e-5f);
	run(*scene, 100);
	EXPECT_NEAR(translationOf(entity).y(), 2.f, 0.05f);
	scene->onEndRuntime();
}

TEST_F(BehaviourTest, ADisabledOscillatorStaysAtRest) {
	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Lamp");
	entity.addComponent<component::Oscillator>().enabled = false;
	scene->onStartRuntime();
	run(*scene, 30);
	EXPECT_NEAR(translationOf(entity).x(), 0.f, 1e-6f);
}

TEST_F(BehaviourTest, AKinematicBodyIsMovedThroughThePhysicsStep) {
	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Platform");
	auto& body = entity.addComponent<component::PhysicBody>();
	body.body.type = SceneBody::BodyType::Kinematic;
	auto& oscillator = entity.addComponent<component::Oscillator>();
	oscillator.offset = {4.f, 0.f, 0.f};
	oscillator.period = 4.f;
	scene->onStartRuntime();
	ASSERT_TRUE(physics::PhysicCommand::isInitialized(*scene));
	run(*scene, 100, 10);
	EXPECT_GT(translationOf(entity).x(), 1.5f);
	EXPECT_LT(translationOf(entity).x(), 4.5f);
	run(*scene, 300, 10);
	EXPECT_GE(translationOf(entity).x(), -0.5f);
	EXPECT_LE(translationOf(entity).x(), 4.5f);
	scene->onEndRuntime();
}

TEST_F(BehaviourTest, APatrolWalksItsWaypointsBackAndForthAndWaits) {
	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Villager");
	auto& patrol = entity.addComponent<component::Patrol>();
	patrol.waypoints = {{0.f, 0.f, 0.f}, {2.f, 0.f, 0.f}};
	patrol.speed = 1.f;
	patrol.waitTime = 1.f;
	patrol.faceDirection = true;
	scene->onStartRuntime();
	run(*scene, 100);
	EXPECT_NEAR(translationOf(entity).x(), 1.f, 0.05f);
	run(*scene, 110);
	EXPECT_NEAR(translationOf(entity).x(), 2.f, 1e-4f);
	run(*scene, 50);
	EXPECT_NEAR(translationOf(entity).x(), 2.f, 1e-4f);
	run(*scene, 100);
	EXPECT_LT(translationOf(entity).x(), 2.f);
	EXPECT_LT(entity.getComponent<component::Transform>().transform.scale().x(), 0.f);
}

TEST_F(BehaviourTest, ALoopPatrolReturnsToTheFirstWaypointAndAWandererStaysInItsRadius) {
	auto scene = mkShared<Scene>();
	auto looper = scene->createEntity("Looper");
	auto& loop = looper.addComponent<component::Patrol>();
	loop.waypoints = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}};
	loop.mode = component::Patrol::Mode::Loop;
	loop.speed = 10.f;
	loop.waitTime = 0.f;
	auto wanderer = scene->createEntity("Wanderer");
	wanderer.getComponent<component::Transform>().transform.translation() = {5.f, 5.f, 0.f};
	auto& wander = wanderer.addComponent<component::Patrol>();
	wander.wanderRadius = 2.f;
	wander.seed = 4.f;
	wander.speed = 3.f;
	wander.waitTime = 0.1f;
	scene->onStartRuntime();
	bool moved = false;
	for (int frame = 0; frame < 400; ++frame) {
		run(*scene, 1);
		const auto offset = translationOf(wanderer) - math::vec3{5.f, 5.f, 0.f};
		EXPECT_LE(std::sqrt(offset.x() * offset.x() + offset.y() * offset.y()), 2.f + 1e-3f);
		moved = moved || std::abs(offset.x()) > 0.01f;
	}
	EXPECT_TRUE(moved);
	EXPECT_LE(translationOf(looper).x(), 1.f + 1e-4f);
}

TEST_F(BehaviourTest, ADriftWrapsAroundItsRangeAndFollowsTheCamera) {
	auto scene = mkShared<Scene>();
	auto cloud = scene->createEntity("Cloud");
	cloud.getComponent<component::Transform>().transform.translation() = {1.f, 2.f, 0.5f};
	auto& drift = cloud.addComponent<component::Drift>();
	drift.velocity = {1.f, 0.f, 0.f};
	drift.range = {2.f, 0.f, 0.f};
	drift.cameraFollow = 0.5f;
	auto camera = scene->createEntity("Camera");
	camera.addComponent<component::Camera>().primary = true;
	camera.getComponent<component::Transform>().transform.translation() = {10.f, 4.f, 0.f};
	scene->onStartRuntime();
	float lowest = 1e9f;
	float highest = -1e9f;
	for (int frame = 0; frame < 500; ++frame) {
		run(*scene, 1);
		lowest = std::min(lowest, translationOf(cloud).x());
		highest = std::max(highest, translationOf(cloud).x());
	}
	EXPECT_NEAR(lowest, 1.f + 5.f - 2.f, 0.05f);
	EXPECT_NEAR(highest, 1.f + 5.f + 2.f, 0.05f);
	EXPECT_NEAR(translationOf(cloud).y(), 2.f + 2.f, 1e-4f);
	EXPECT_NEAR(translationOf(cloud).z(), 0.5f, 1e-6f);
}

TEST_F(BehaviourTest, AFollowTargetKeepsItsOffsetAndTurnsWithTheTarget) {
	auto scene = mkShared<Scene>();
	auto player = scene->createEntity("Player");
	auto& playerTransform = player.getComponent<component::Transform>().transform;
	playerTransform.translation() = {3.f, 1.f, -2.f};
	playerTransform.rotation() = {0.4f, 0.f, 0.f};
	auto camera = scene->createEntity("MinimapCamera");
	camera.getComponent<component::Transform>().transform.rotation() = {-1.5f, 0.25f, 0.f};
	auto& follow = camera.addComponent<component::FollowTarget>();
	follow.target = player.getUUID();
	follow.offset = {0.f, 30.f, 0.f};
	scene->onStartRuntime();
	run(*scene, 1);
	const auto& placed = camera.getComponent<component::Transform>().transform;
	EXPECT_NEAR(placed.translation().x(), 3.f, 1e-5f);
	EXPECT_NEAR(placed.translation().y(), 31.f, 1e-5f);
	EXPECT_NEAR(placed.translation().z(), -2.f, 1e-5f);
	EXPECT_NEAR(placed.rotation().x(), -1.5f, 1e-6f);
	EXPECT_NEAR(placed.rotation().y(), 0.25f, 1e-6f);
	playerTransform.translation() = {5.f, 2.f, 0.f};
	playerTransform.rotation().y() = 1.f;
	run(*scene, 1);
	EXPECT_NEAR(translationOf(camera).x(), 5.f, 1e-5f);
	EXPECT_NEAR(translationOf(camera).y(), 32.f, 1e-5f);
	EXPECT_NEAR(camera.getComponent<component::Transform>().transform.rotation().y(), 1.25f, 1e-6f);
	EXPECT_NEAR(camera.getComponent<component::Transform>().transform.rotation().x(), -1.5f, 1e-6f);
}

TEST_F(BehaviourTest, AFollowTargetOffOrWithoutTargetStaysWhereItIs) {
	auto scene = mkShared<Scene>();
	auto player = scene->createEntity("Player");
	player.getComponent<component::Transform>().transform.translation() = {3.f, 1.f, -2.f};
	auto still = scene->createEntity("Still");
	still.getComponent<component::Transform>().transform.translation() = {7.f, 0.f, 0.f};
	auto& off = still.addComponent<component::FollowTarget>();
	off.target = player.getUUID();
	off.enabled = false;
	auto lost = scene->createEntity("Lost");
	lost.addComponent<component::FollowTarget>().target = core::UUID{123456789};
	auto self = scene->createEntity("Self");
	self.addComponent<component::FollowTarget>().target = self.getUUID();
	scene->onStartRuntime();
	run(*scene, 3);
	EXPECT_NEAR(translationOf(still).x(), 7.f, 1e-6f);
	EXPECT_NEAR(translationOf(lost).y(), 0.f, 1e-6f);
	EXPECT_NEAR(translationOf(self).y(), 0.f, 1e-6f);
}

TEST_F(BehaviourTest, TheSunRisesCulminatesAndSetsOnItsPath) {
	component::DayNightCycle cycle;
	cycle.azimuth = 90.f;
	cycle.maxElevation = 60.f;
	cycle.timeOfDay = 6.f;
	EXPECT_NEAR(cycle.getSunDirection().x(), 1.f, 1e-4f);
	EXPECT_NEAR(cycle.getSunDirection().y(), 0.f, 1e-4f);
	cycle.timeOfDay = 12.f;
	EXPECT_NEAR(cycle.getSunDirection().y(), std::sin(60.f * std::numbers::pi_v<float> / 180.f), 1e-4f);
	EXPECT_NEAR(cycle.getSunDirection().x(), 0.f, 1e-4f);
	cycle.timeOfDay = 18.f;
	EXPECT_NEAR(cycle.getSunDirection().x(), -1.f, 1e-4f);
	cycle.timeOfDay = 0.f;
	EXPECT_LT(cycle.getSunDirection().y(), -0.8f);
	cycle.azimuth = 0.f;
	cycle.timeOfDay = 6.f;
	EXPECT_NEAR(cycle.getSunDirection().z(), -1.f, 1e-4f);
}

TEST_F(BehaviourTest, ADayNightCycleTurnsItsLightWithTheSunThenTheMoonAndAdvancesTheHours) {
	auto scene = mkShared<Scene>();
	auto sunEntity = scene->createEntity("Sun");
	auto& light = sunEntity.addComponent<component::Light>();
	light.type = component::Light::Type::Directional;
	auto sky = scene->createEntity("Sky");
	sky.addComponent<component::Environment>();
	sky.addComponent<component::ProceduralSky>();
	auto& cycle = sky.addComponent<component::DayNightCycle>();
	cycle.sun = sunEntity.getUUID();
	cycle.timeOfDay = 12.f;
	cycle.dayLength = 240.f;
	auto camera = scene->createEntity("Camera");
	camera.addComponent<component::Camera>().primary = true;
	scene->onStartRuntime();
	run(*scene, 1);
	const auto& noon = sky.getComponent<component::DayNightCycle>();
	EXPECT_GT(noon.timeOfDay, 12.f);
	EXPECT_LT(noon.timeOfDay, 12.01f);
	const auto direction = [&sunEntity]() -> math::vec3 {
		const auto forward =
				sunEntity.getComponent<component::Transform>().transform() * math::vec4{0.f, 0.f, -1.f, 0.f};
		return {forward.x(), forward.y(), forward.z()};
	};
	const math::vec3 toSun = noon.getSunDirection();
	EXPECT_NEAR(direction().x(), -toSun.x(), 1e-3f);
	EXPECT_NEAR(direction().y(), -toSun.y(), 1e-3f);
	EXPECT_NEAR(direction().z(), -toSun.z(), 1e-3f);
	EXPECT_NEAR(light.intensity, noon.sunIlluminance, 1.f);
	EXPECT_NEAR(sky.getComponent<component::ProceduralSky>().sunDirection.y(), toSun.y(), 1e-6f);
	EXPECT_NEAR(camera.getComponent<component::Camera>().camera.getPostProcess().exposure, 0.f, 1e-5f);
	sky.getComponent<component::DayNightCycle>().timeOfDay = 0.f;
	run(*scene, 1);
	EXPECT_LT(direction().y(), -0.5f);
	EXPECT_NEAR(light.intensity, noon.moonIlluminance, 1.f);
	EXPECT_NEAR(camera.getComponent<component::Camera>().camera.getPostProcess().exposure, noon.nightExposure, 1e-4f);
	sky.getComponent<component::DayNightCycle>().enabled = false;
	sky.getComponent<component::DayNightCycle>().timeOfDay = 23.99f;
	run(*scene, 10);
	EXPECT_NEAR(sky.getComponent<component::DayNightCycle>().timeOfDay, 23.99f, 1e-5f);
	sky.getComponent<component::DayNightCycle>().enabled = true;
	sky.getComponent<component::DayNightCycle>().dayLength = 1.f;
	run(*scene, 10);
	EXPECT_LT(sky.getComponent<component::DayNightCycle>().timeOfDay, 12.f);
}

TEST_F(BehaviourTest, AFirstPersonControllerFallsAndTriggersSeeItsBodyIn3D) {
	auto scene = mkShared<Scene>();
	auto walker = scene->createEntity("Walker");
	walker.getComponent<component::Transform>().transform.translation() = {0.f, 3.f, 0.f};
	auto& controller = walker.addComponent<component::FirstPersonController>();
	controller.captureCursor = false;
	auto camera = scene->createEntity("Camera");
	camera.addComponent<component::Camera>().primary = true;
	auto chest = scene->createEntity("Chest");
	auto& chestTransform = chest.getComponent<component::Transform>().transform;
	chestTransform.translation() = {0.f, 0.5f, 0.f};
	chestTransform.scale() = {1.f, 1.f, 1.f};
	chest.addComponent<component::Trigger>().trigger.type = SceneTrigger::TriggerType::Target;
	auto far = scene->createEntity("Far");
	far.getComponent<component::Transform>().transform.translation() = {0.f, 0.5f, 20.f};
	far.addComponent<component::Trigger>().trigger.type = SceneTrigger::TriggerType::Target;
	scene->onStartRuntime();
	run(*scene, 30);
	EXPECT_LT(translationOf(walker).y(), 3.f);
	EXPECT_TRUE(chest.getComponent<component::Trigger>().trigger.wasOverlapping());
	EXPECT_FALSE(far.getComponent<component::Trigger>().trigger.wasOverlapping());
	walker.getComponent<component::FirstPersonController>().enabled = false;
	const float stopped = translationOf(walker).y();
	run(*scene, 10);
	EXPECT_NEAR(translationOf(walker).y(), stopped, 1e-6f);
}
