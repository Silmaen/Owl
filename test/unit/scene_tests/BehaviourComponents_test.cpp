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

	static auto positionOf(const Entity& iEntity) -> math::vec3 {
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
	EXPECT_NEAR(positionOf(entity).y(), 5.f, 0.05f);
	EXPECT_NEAR(positionOf(entity).x(), 1.f, 1e-5f);
	run(*scene, 100);
	EXPECT_NEAR(positionOf(entity).y(), 2.f, 0.05f);
	scene->onEndRuntime();
}

TEST_F(BehaviourTest, ADisabledOscillatorStaysAtRest) {
	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Lamp");
	entity.addComponent<component::Oscillator>().enabled = false;
	scene->onStartRuntime();
	run(*scene, 30);
	EXPECT_NEAR(positionOf(entity).x(), 0.f, 1e-6f);
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
	EXPECT_GT(positionOf(entity).x(), 1.5f);
	EXPECT_LT(positionOf(entity).x(), 4.5f);
	run(*scene, 300, 10);
	EXPECT_GE(positionOf(entity).x(), -0.5f);
	EXPECT_LE(positionOf(entity).x(), 4.5f);
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
	EXPECT_NEAR(positionOf(entity).x(), 1.f, 0.05f);
	run(*scene, 110);
	EXPECT_NEAR(positionOf(entity).x(), 2.f, 1e-4f);
	run(*scene, 50);
	EXPECT_NEAR(positionOf(entity).x(), 2.f, 1e-4f);
	run(*scene, 100);
	EXPECT_LT(positionOf(entity).x(), 2.f);
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
		const auto offset = positionOf(wanderer) - math::vec3{5.f, 5.f, 0.f};
		EXPECT_LE(std::sqrt(offset.x() * offset.x() + offset.y() * offset.y()), 2.f + 1e-3f);
		moved = moved || std::abs(offset.x()) > 0.01f;
	}
	EXPECT_TRUE(moved);
	EXPECT_LE(positionOf(looper).x(), 1.f + 1e-4f);
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
		lowest = std::min(lowest, positionOf(cloud).x());
		highest = std::max(highest, positionOf(cloud).x());
	}
	EXPECT_NEAR(lowest, 1.f + 5.f - 2.f, 0.05f);
	EXPECT_NEAR(highest, 1.f + 5.f + 2.f, 0.05f);
	EXPECT_NEAR(positionOf(cloud).y(), 2.f + 2.f, 1e-4f);
	EXPECT_NEAR(positionOf(cloud).z(), 0.5f, 1e-6f);
}
