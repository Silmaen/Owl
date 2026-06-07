/**
 * @file Camera3DController_test.cpp
 * @author Silmaen
 * @date 06/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <input/Input.h>
#include <renderer/Camera3DController.h>

using namespace owl;
using namespace owl::input;
using namespace owl::renderer;

TEST(Camera3DController, DefaultsAndSetters) {
	Camera3DController cam;
	EXPECT_EQ(cam.getPosition().x(), 0.0f);
	EXPECT_EQ(cam.getYaw(), 0.0f);
	EXPECT_EQ(cam.getPitch(), 0.0f);
	EXPECT_NEAR(cam.getMoveSpeed(), 8.0f, 0.001);
	EXPECT_NEAR(cam.getLookSpeed(), 1.5f, 0.001);

	cam.setPosition({1.0f, 2.0f, 3.0f});
	cam.setMoveSpeed(4.0f);
	cam.setLookSpeed(2.0f);
	EXPECT_EQ(cam.getPosition().y(), 2.0f);
	EXPECT_NEAR(cam.getMoveSpeed(), 4.0f, 0.001);
	EXPECT_NEAR(cam.getLookSpeed(), 2.0f, 0.001);
}

TEST(Camera3DController, BasisAtIdentity) {
	const Camera3DController cam;
	const auto fwd = cam.getForwardDirection();
	EXPECT_NEAR(fwd.x(), 0.0f, 0.001);
	EXPECT_NEAR(fwd.y(), 0.0f, 0.001);
	EXPECT_NEAR(fwd.z(), -1.0f, 0.001);
	const auto right = cam.getRightDirection();
	EXPECT_NEAR(right.x(), 1.0f, 0.001);
	EXPECT_NEAR(right.z(), 0.0f, 0.001);
	const auto up = cam.getUpDirection();
	EXPECT_NEAR(up.y(), 1.0f, 0.001);
}

TEST(Camera3DController, MoveLocalForwardRightUp) {
	Camera3DController cam;
	cam.moveLocal({0.0f, 0.0f, 1.0f}, 1.0f);
	EXPECT_NEAR(cam.getPosition().z(), -1.0f, 0.001);
	cam.setPosition({0.0f, 0.0f, 0.0f});
	cam.moveLocal({1.0f, 0.0f, 0.0f}, 2.0f);
	EXPECT_NEAR(cam.getPosition().x(), 2.0f, 0.001);
	cam.setPosition({0.0f, 0.0f, 0.0f});
	cam.moveLocal({0.0f, 1.0f, 0.0f}, 3.0f);
	EXPECT_NEAR(cam.getPosition().y(), 3.0f, 0.001);
}

TEST(Camera3DController, MoveLocalRespectsYaw) {
	Camera3DController cam;
	cam.setYaw(math::radians(90.0f));
	cam.moveLocal({0.0f, 0.0f, 1.0f}, 1.0f);
	EXPECT_NEAR(cam.getPosition().x(), -1.0f, 0.001);
	EXPECT_NEAR(cam.getPosition().z(), 0.0f, 0.001);
}

TEST(Camera3DController, MoveLocalZeroAxesNoMotion) {
	Camera3DController cam;
	cam.setPosition({5.0f, 6.0f, 7.0f});
	cam.moveLocal({0.0f, 0.0f, 0.0f}, 10.0f);
	EXPECT_NEAR(cam.getPosition().x(), 5.0f, 0.001);
	EXPECT_NEAR(cam.getPosition().y(), 6.0f, 0.001);
	EXPECT_NEAR(cam.getPosition().z(), 7.0f, 0.001);
}

TEST(Camera3DController, PitchIsClamped) {
	Camera3DController cam;
	cam.addPitch(10.0f);
	EXPECT_NEAR(cam.getPitch(), 1.5f, 0.001);
	cam.addPitch(-100.0f);
	EXPECT_NEAR(cam.getPitch(), -1.5f, 0.001);
	cam.setPitch(0.5f);
	EXPECT_NEAR(cam.getPitch(), 0.5f, 0.001);
}

TEST(Camera3DController, EulerRoundTrip) {
	Camera3DController cam;
	cam.setEulerRotation({0.3f, 1.2f, 0.0f});
	EXPECT_NEAR(cam.getPitch(), 0.3f, 0.001);
	EXPECT_NEAR(cam.getYaw(), 1.2f, 0.001);
	const auto euler = cam.getEulerRotation();
	EXPECT_NEAR(euler.x(), 0.3f, 0.001);
	EXPECT_NEAR(euler.y(), 1.2f, 0.001);
	EXPECT_NEAR(euler.z(), 0.0f, 0.001);
}

TEST(Camera3DController, TransformAndView) {
	Camera3DController cam;
	cam.setPosition({1.0f, 2.0f, 3.0f});
	const auto transform = cam.getTransform();
	EXPECT_NEAR(transform.translation().x(), 1.0f, 0.001);
	EXPECT_NEAR(transform.translation().y(), 2.0f, 0.001);
	EXPECT_NEAR(transform.translation().z(), 3.0f, 0.001);
	const auto view = cam.getViewMatrix();
	EXPECT_NEAR(view(0, 3), -1.0f, 0.001);
	EXPECT_NEAR(view(1, 3), -2.0f, 0.001);
	EXPECT_NEAR(view(2, 3), -3.0f, 0.001);
}

TEST(Camera3DController, OnUpdateKeyboard) {
	Input::init(window::Type::Null);
	core::Timestep ts;
	const core::Timestep::duration delta = std::chrono::milliseconds(10);
	ts.forceUpdate(delta);

	Camera3DController cam;
	{
		Input::injectKey(key::W);
		ts.forceUpdate(delta);
		cam.onUpdate(ts);
		EXPECT_NEAR(cam.getPosition().z(), -0.08f, 0.001);
		Input::resetInjection();
	}
	cam.setPosition({0.0f, 0.0f, 0.0f});
	{
		Input::injectKey(key::Space);
		ts.forceUpdate(delta);
		cam.onUpdate(ts);
		EXPECT_NEAR(cam.getPosition().y(), 0.08f, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::Right);
		ts.forceUpdate(delta);
		cam.onUpdate(ts);
		EXPECT_NEAR(cam.getYaw(), -0.015f, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::Up);
		ts.forceUpdate(delta);
		cam.onUpdate(ts);
		EXPECT_NEAR(cam.getPitch(), 0.015f, 0.001);
		Input::resetInjection();
	}
}
