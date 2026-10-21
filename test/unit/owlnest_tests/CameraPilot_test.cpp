/**
 * @file CameraPilot_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/CameraPilot.h"

#include <math/linAlgebra.h>

#include <gtest/gtest.h>

using owl::nest::panel::CameraPilot;
using owl::renderer::CameraEditor;
using Step = CameraPilot::Step;

namespace {
auto makeView() -> CameraEditor { return {45.f, 1.5f, 0.1f, 100.f}; }
}// namespace

TEST(CameraPilot, StopGivesBackTheSavedView) {
	CameraPilot pilot;
	EXPECT_FALSE(pilot.isActive());
	EXPECT_FALSE(pilot.stop().has_value());
	CameraEditor saved = makeView();
	saved.lookFrom({1.f, 2.f, 3.f}, {0.f, 0.f, -1.f});
	const CameraEditor view = makeView();
	pilot.start(owl::core::UUID{42}, saved, view);
	EXPECT_TRUE(pilot.isActive());
	EXPECT_EQ(pilot.getEntity(), owl::core::UUID{42});
	const auto back = pilot.stop();
	ASSERT_TRUE(back.has_value());
	EXPECT_FLOAT_EQ(back->getPosition().x(), saved.getPosition().x());
	EXPECT_FALSE(pilot.isActive());
	EXPECT_EQ(pilot.getEntity(), owl::core::UUID{0});
}

TEST(CameraPilot, AGestureBeginsMovesAndCommitsOnRelease) {
	CameraPilot pilot;
	CameraEditor view = makeView();
	pilot.start(owl::core::UUID{7}, view, view);
	EXPECT_TRUE(pilot.shouldFollowEntity(view));
	EXPECT_EQ(pilot.track(view, false), Step::Idle);

	view.lookFrom({0.f, 0.f, 5.f}, {0.f, 0.f, -1.f});
	EXPECT_FALSE(pilot.shouldFollowEntity(view));
	EXPECT_EQ(pilot.track(view, true), Step::Begin);
	EXPECT_TRUE(pilot.isGestureActive());
	EXPECT_FALSE(pilot.shouldFollowEntity(view));

	view.lookFrom({0.f, 1.f, 5.f}, {0.f, 0.f, -1.f});
	EXPECT_EQ(pilot.track(view, true), Step::Move);
	// Held still with the button down: the gesture goes on.
	EXPECT_EQ(pilot.track(view, true), Step::Idle);
	EXPECT_EQ(pilot.track(view, false), Step::Commit);
	EXPECT_FALSE(pilot.isGestureActive());
	EXPECT_TRUE(pilot.shouldFollowEntity(view));
	EXPECT_EQ(pilot.track(view, false), Step::Idle);
}

TEST(CameraPilot, AWheelStepIsAGestureOfItsOwn) {
	CameraPilot pilot;
	CameraEditor view = makeView();
	pilot.start(owl::core::UUID{7}, view, view);
	view.lookFrom({0.f, 0.f, 4.f}, {0.f, 0.f, -1.f});
	EXPECT_EQ(pilot.track(view, false), Step::Begin);
	EXPECT_EQ(pilot.track(view, false), Step::Commit);
}

TEST(CameraPilot, FollowingTheEntityIsNoGesture) {
	CameraPilot pilot;
	CameraEditor view = makeView();
	pilot.start(owl::core::UUID{7}, view, view);
	// An undo moved the camera: the view follows it, then nothing is recorded.
	view.lookFrom({3.f, 0.f, 2.f}, {1.f, 0.f, 0.f});
	pilot.markFollowed(view);
	EXPECT_EQ(pilot.track(view, false), Step::Idle);
}

TEST(CameraPilot, PerspectiveCameraTakesTheEyeAndTheFacing) {
	CameraEditor view = makeView();
	view.lookFrom({1.f, 2.f, 3.f}, {1.f, 0.f, 0.f});
	const owl::math::Transform current{{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {2.f, 2.f, 2.f}};
	const auto world = CameraPilot::getCameraWorld(view, current, false);
	EXPECT_NEAR(world.translation().x(), 1.f, 1e-4f);
	EXPECT_NEAR(world.translation().y(), 2.f, 1e-4f);
	EXPECT_NEAR(world.translation().z(), 3.f, 1e-4f);
	EXPECT_FLOAT_EQ(world.scale().x(), 2.f);
	owl::math::Transform rigid = world;
	rigid.scale() = {1.f, 1.f, 1.f};
	const owl::math::vec4 forward = rigid() * owl::math::vec4{0.f, 0.f, -1.f, 0.f};
	EXPECT_NEAR(forward.x(), 1.f, 1e-4f);
	EXPECT_NEAR(forward.y(), 0.f, 1e-4f);
	EXPECT_NEAR(forward.z(), 0.f, 1e-4f);
}

TEST(CameraPilot, OrthographicCameraSlidesAcrossThePlane) {
	CameraEditor view = makeView();
	view.lookFrom({4.f, -2.f, 20.f}, {0.f, 0.f, -1.f});
	const owl::math::Transform current{{0.f, 0.f, 5.f}, {0.f, 0.f, 0.3f}, {1.f, 1.f, 1.f}};
	const auto world = CameraPilot::getCameraWorld(view, current, true);
	EXPECT_NEAR(world.translation().x(), 4.f, 1e-4f);
	EXPECT_NEAR(world.translation().y(), -2.f, 1e-4f);
	EXPECT_FLOAT_EQ(world.translation().z(), 5.f);
	EXPECT_FLOAT_EQ(world.rotation().z(), 0.3f);
}
