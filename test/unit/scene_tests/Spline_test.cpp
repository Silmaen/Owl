/**
 * @file Spline_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <chrono>
#include <cmath>
#include <numbers>

using namespace owl;
using namespace owl::scene;

namespace {

class SplineTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames, const int iMilliseconds = 100) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(iMilliseconds));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto translationOf(const Entity& iEntity) -> math::vec3 {
		return iEntity.getComponent<component::Transform>().transform.translation();
	}

	// A straight rail of 10 units along +X, with a lane one unit to its right.
	static auto addRail(Scene& ioScene) -> Entity {
		auto rail = ioScene.createEntity("Rail");
		auto& spline = rail.addComponent<component::Spline>();
		spline.curve.setInterpolation(math::SplineInterpolation::Linear);
		spline.curve.setPoints({{.position = {0.f, 0.f, 0.f}}, {.position = {10.f, 0.f, 0.f}}});
		spline.lanes.push_back({.name = "right", .offset = 1.f, .height = 0.5f});
		return rail;
	}
};

}// namespace

TEST_F(SplineTest, LanesAreBesideTheCurve) {
	component::Spline spline;
	spline.curve.setInterpolation(math::SplineInterpolation::Linear);
	spline.curve.setPoints({{.position = {0.f, 0.f, 0.f}}, {.position = {10.f, 0.f, 0.f}}});
	spline.lanes.push_back({.name = "right", .offset = 2.f, .height = 1.f});
	ASSERT_NE(spline.findLane("right"), nullptr);
	EXPECT_EQ(spline.findLane("left"), nullptr);
	// Along +X with Y up, the right is +Z.
	const auto lane = spline.sample(4.f, "right");
	EXPECT_NEAR(lane.position.x(), 4.f, 1e-4f);
	EXPECT_NEAR(lane.position.y(), 1.f, 1e-4f);
	EXPECT_NEAR(lane.position.z(), 2.f, 1e-4f);
	const auto unknown = spline.sample(4.f, "left", -1.f);
	EXPECT_NEAR(unknown.position.z(), -1.f, 1e-4f);
	// A 2D path: up along Z, the right of +X is -Y.
	spline.up = {0.f, 0.f, 1.f};
	EXPECT_NEAR(spline.sample(4.f, "right").position.y(), -2.f, 1e-4f);
	EXPECT_FLOAT_EQ(spline.getLateral({0.f, 0.f, 1.f}).normSq(), 0.f);
}

TEST_F(SplineTest, RoundTripsThroughTheSceneFile) {
	auto scene = mkShared<Scene>();
	auto rail = scene->createEntity("Rail");
	auto& spline = rail.addComponent<component::Spline>();
	spline.curve.setInterpolation(math::SplineInterpolation::Bezier);
	spline.curve.setClosed(true);
	spline.curve.setResolution(8);
	spline.curve.setPoints({{.position = {0.f, 0.f, 0.f}, .tangentIn = {-1.f, 0.f, 0.f}, .tangentOut = {1.f, 0.f, 0.f}},
							{.position = {4.f, 2.f, 0.f}, .tangentIn = {0.f, -1.f, 0.f}, .tangentOut = {0.f, 1.f, 0.f}},
							{.position = {0.f, 4.f, 1.f}}});
	spline.up = {0.f, 0.f, 1.f};
	spline.lanes.push_back({.name = "inner", .offset = -0.5f, .height = 0.f});
	auto cart = scene->createEntity("Cart");
	auto& follower = cart.addComponent<component::SplineFollower>();
	follower.spline = rail.getUUID();
	follower.lane = "inner";
	follower.offset = 0.25f;
	follower.speed = -3.f;
	follower.startDistance = 2.f;
	follower.mode = component::SplineFollower::Mode::PingPong;
	follower.orient = false;
	follower.enabled = false;

	const auto text = SceneSerializer(scene).serializeToString();
	auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer({text.begin(), text.end()}));
	const auto readRail = loaded->findEntityByUUID(rail.getUUID());
	ASSERT_TRUE(readRail && readRail.hasComponent<component::Spline>());
	const auto& read = readRail.getComponent<component::Spline>();
	EXPECT_TRUE(read.curve == spline.curve);
	EXPECT_EQ(read.lanes, spline.lanes);
	EXPECT_FLOAT_EQ(read.up.z(), 1.f);
	EXPECT_NEAR(read.curve.getLength(), spline.curve.getLength(), 1e-4f);
	const auto readCart = loaded->findEntityByUUID(cart.getUUID());
	ASSERT_TRUE(readCart && readCart.hasComponent<component::SplineFollower>());
	const auto& readFollower = readCart.getComponent<component::SplineFollower>();
	EXPECT_EQ(readFollower.spline, rail.getUUID());
	EXPECT_EQ(readFollower.lane, "inner");
	EXPECT_FLOAT_EQ(readFollower.offset, 0.25f);
	EXPECT_FLOAT_EQ(readFollower.speed, -3.f);
	EXPECT_FLOAT_EQ(readFollower.startDistance, 2.f);
	EXPECT_EQ(readFollower.mode, component::SplineFollower::Mode::PingPong);
	EXPECT_FALSE(readFollower.orient);
	EXPECT_FALSE(readFollower.enabled);
}

TEST_F(SplineTest, AFollowerRidesItsLaneAndLoops) {
	auto scene = mkShared<Scene>();
	const auto rail = addRail(*scene);
	auto cart = scene->createEntity("Cart");
	auto& follower = cart.addComponent<component::SplineFollower>();
	follower.spline = rail.getUUID();
	follower.lane = "right";
	follower.speed = 2.f;
	follower.startDistance = 1.f;
	scene->onStartRuntime();
	run(*scene, 1);
	EXPECT_NEAR(translationOf(cart).x(), 1.f, 1e-3f);
	EXPECT_NEAR(translationOf(cart).z(), 1.f, 1e-3f);
	EXPECT_NEAR(translationOf(cart).y(), 0.5f, 1e-3f);
	// Facing +X: -Z turned a quarter towards +X, a yaw of -pi/2.
	EXPECT_NEAR(cart.getComponent<component::Transform>().transform.rotation().y(), -std::numbers::pi_v<float> / 2.f,
				1e-3f);
	run(*scene, 10);
	EXPECT_NEAR(translationOf(cart).x(), 3.f, 1e-3f);
	run(*scene, 45);
	EXPECT_NEAR(translationOf(cart).x(), 2.f, 1e-2f);
	scene->onEndRuntime();
}

TEST_F(SplineTest, OnceStopsAtTheEndAndPingPongTurnsRound) {
	auto scene = mkShared<Scene>();
	const auto rail = addRail(*scene);
	rail.getComponent<component::Transform>().transform.translation() = {0.f, 5.f, 0.f};
	auto once = scene->createEntity("Once");
	auto& stop = once.addComponent<component::SplineFollower>();
	stop.spline = rail.getUUID();
	stop.mode = component::SplineFollower::Mode::Once;
	stop.speed = 4.f;
	stop.orient = false;
	auto bounce = scene->createEntity("Bounce");
	auto& pingPong = bounce.addComponent<component::SplineFollower>();
	pingPong.spline = rail.getUUID();
	pingPong.mode = component::SplineFollower::Mode::PingPong;
	pingPong.speed = 4.f;
	scene->onStartRuntime();
	run(*scene, 31);
	EXPECT_NEAR(translationOf(once).x(), 10.f, 1e-3f);
	EXPECT_NEAR(translationOf(once).y(), 5.f, 1e-3f);
	// 12 units: to the end and 2 back.
	EXPECT_NEAR(translationOf(bounce).x(), 8.f, 1e-2f);
	scene->onEndRuntime();
}

TEST_F(SplineTest, AFollowerUnderAParentAndOnAClosedLoopOnce) {
	auto scene = mkShared<Scene>();
	auto rail = scene->createEntity("Loop");
	auto& spline = rail.addComponent<component::Spline>();
	spline.curve.setInterpolation(math::SplineInterpolation::Linear);
	spline.curve.setClosed(true);
	spline.curve.setPoints({{.position = {0.f, 0.f, 0.f}}, {.position = {4.f, 0.f, 0.f}},
							{.position = {4.f, 4.f, 0.f}}, {.position = {0.f, 4.f, 0.f}}});
	spline.up = {0.f, 0.f, 1.f};
	auto holder = scene->createEntity("Holder");
	holder.getComponent<component::Transform>().transform.translation() = {10.f, 0.f, 0.f};
	auto cart = scene->createEntity("Cart");
	scene->setParent(cart, holder);
	auto& follower = cart.addComponent<component::SplineFollower>();
	follower.spline = rail.getUUID();
	follower.mode = component::SplineFollower::Mode::Once;
	follower.speed = 4.f;
	follower.startDistance = 2.f;
	scene->onStartRuntime();
	run(*scene, 1);
	EXPECT_NEAR(scene->getWorldTransform(cart).translation().x(), 2.f, 1e-3f);
	EXPECT_NEAR(translationOf(cart).x(), -8.f, 1e-3f);
	// On a 2D path the +X axis follows the curve: no turn along the first side.
	EXPECT_NEAR(cart.getComponent<component::Transform>().transform.rotation().z(), 0.f, 1e-3f);
	run(*scene, 8);
	EXPECT_NEAR(scene->getWorldTransform(cart).translation().y(), 1.2f, 1e-2f);
	EXPECT_NEAR(cart.getComponent<component::Transform>().transform.rotation().z(), std::numbers::pi_v<float> / 2.f,
				1e-3f);
	// A lap of 16 units, then it stops where it started.
	run(*scene, 60);
	EXPECT_NEAR(scene->getWorldTransform(cart).translation().x(), 2.f, 1e-2f);
	EXPECT_NEAR(scene->getWorldTransform(cart).translation().y(), 0.f, 1e-2f);
	scene->onEndRuntime();
}

TEST_F(SplineTest, AFollowerWithoutSplineOrDisabledStaysPut) {
	auto scene = mkShared<Scene>();
	const auto rail = addRail(*scene);
	auto lost = scene->createEntity("Lost");
	lost.getComponent<component::Transform>().transform.translation() = {7.f, 7.f, 7.f};
	lost.addComponent<component::SplineFollower>().spline = core::UUID{123456};
	auto off = scene->createEntity("Off");
	auto& follower = off.addComponent<component::SplineFollower>();
	follower.spline = rail.getUUID();
	follower.enabled = false;
	auto self = scene->createEntity("Self");
	self.addComponent<component::Spline>();
	self.addComponent<component::SplineFollower>().spline = self.getUUID();
	scene->onStartRuntime();
	run(*scene, 5);
	EXPECT_NEAR(translationOf(lost).x(), 7.f, 1e-5f);
	EXPECT_NEAR(translationOf(off).x(), 0.f, 1e-5f);
	scene->onEndRuntime();
}
