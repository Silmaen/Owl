/**
 * @file ShipController_test.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <input/Input.h>
#include <input/KeyCodes.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <window/Window.h>

#include <chrono>
#include <cmath>

using namespace owl;
using namespace owl::scene;

namespace {

class ShipControllerTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		input::Input::init(window::Type::Null);
		ship = scene.createEntity("Ship");
		auto& controller = ship.addComponent<component::ShipController>();
		controller.captureCursor = false;
		scene.onStartRuntime();
	}

	void TearDown() override {
		scene.onEndRuntime();
		input::Input::invalidate();
		core::Log::invalidate();
	}

	void run(const int iFrames) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(10));
			scene.onUpdateRuntime(step, false);
		}
	}

	// Hold a key for some frames, then release it.
	void hold(const input::KeyCode iKey, const int iFrames) {
		input::Input::injectKey(iKey);
		run(iFrames);
		input::Input::injectKey(iKey);
	}

	auto controller() const -> component::ShipController& { return ship.getComponent<component::ShipController>(); }

	auto pose() const -> math::Transform& { return ship.getComponent<component::Transform>().transform; }

	Scene scene;
	Entity ship;
};

}// namespace

TEST_F(ShipControllerTest, ThrustAcceleratesAlongTheNose) {
	hold(input::key::W, 50);
	EXPECT_NEAR(controller().getSpeed(), 6.f, 0.2f);
	EXPECT_LT(controller().m_velocity.z(), -5.f);
	EXPECT_LT(pose().translation().z(), -1.f);
	EXPECT_NEAR(pose().translation().x(), 0.f, 1e-4f);
}

TEST_F(ShipControllerTest, TheAssistDampsTheDriftAndOffKeepsIt) {
	hold(input::key::W, 50);
	const float cruising = controller().getSpeed();
	run(200);
	EXPECT_LT(controller().getSpeed(), cruising * 0.15f);
	controller().flightAssist = false;
	hold(input::key::W, 50);
	const float drifting = controller().getSpeed();
	run(200);
	EXPECT_NEAR(controller().getSpeed(), drifting, 1e-3f);
}

TEST_F(ShipControllerTest, TheAssistActionTogglesIt) {
	EXPECT_TRUE(controller().flightAssist);
	hold(input::key::Z, 2);
	run(1);
	EXPECT_FALSE(controller().flightAssist);
	hold(input::key::Z, 2);
	EXPECT_TRUE(controller().flightAssist);
}

TEST_F(ShipControllerTest, MaxSpeedCapsTheVelocity) {
	controller().maxSpeed = 2.f;
	hold(input::key::W, 100);
	EXPECT_NEAR(controller().getSpeed(), 2.f, 1e-3f);
}

TEST_F(ShipControllerTest, RollAndPitchTurnTheShip) {
	hold(input::key::D, 50);
	// Rolling right turns the body about its nose: clockwise seen from behind, a negative angle about Z.
	EXPECT_LT(pose().rotation().z(), -0.2f);
	EXPECT_NEAR(pose().rotation().x(), 0.f, 1e-3f);
	run(100);
	const float rolled = pose().rotation().z();
	run(50);
	EXPECT_NEAR(pose().rotation().z(), rolled, 1e-3f);
	pose().rotation() = {0.f, 0.f, 0.f};
	hold(input::key::Up, 50);
	EXPECT_GT(pose().rotation().x(), 0.2f);
	hold(input::key::W, 50);
	EXPECT_GT(controller().m_velocity.y(), 0.5f);
}

TEST_F(ShipControllerTest, AWorldPositionMovesInDoublePrecision) {
	constexpr double au = 149600000000.0;
	scene.shiftWorldOrigin({au, 0.0, 0.0});
	auto& world = ship.addComponent<component::WorldPosition>();
	world.position = {au + 0.25, 0.0, 0.0};
	run(1);
	controller().maxSpeed = 0.f;
	controller().m_velocity = {0.1f, 0.f, 0.f};
	controller().flightAssist = false;
	run(10);
	// One millimetre per frame: a float steps by 16 km at 1 AU, a double by 30 micrometres.
	EXPECT_NEAR(world.position.x() - au, 0.25 + 0.01, 2e-4);
	EXPECT_NEAR(pose().translation().x(), 0.26f, 2e-4f);
}

TEST_F(ShipControllerTest, ADisabledShipStaysStill) {
	controller().enabled = false;
	hold(input::key::W, 30);
	EXPECT_FLOAT_EQ(controller().getSpeed(), 0.f);
	EXPECT_FLOAT_EQ(pose().translation().z(), 0.f);
}

TEST_F(ShipControllerTest, ItWantsTheCursorCaptured) {
	EXPECT_FALSE(scene.wantsCursorCapture());
	controller().captureCursor = true;
	EXPECT_TRUE(scene.wantsCursorCapture());
	controller().enabled = false;
	EXPECT_FALSE(scene.wantsCursorCapture());
}

TEST_F(ShipControllerTest, InvertPitchTurnsTheNoseTheOtherWay) {
	controller().invertPitch = true;
	hold(input::key::Up, 50);
	EXPECT_LT(pose().rotation().x(), -0.2f);
}

TEST_F(ShipControllerTest, WithoutInputTheShipKeepsItsAttitude) {
	pose().rotation() = {0.3f, -0.6f, 0.2f};
	run(1000);
	EXPECT_NEAR(pose().rotation().x(), 0.3f, 1e-3f);
	EXPECT_NEAR(pose().rotation().y(), -0.6f, 1e-3f);
	EXPECT_NEAR(pose().rotation().z(), 0.2f, 1e-3f);
	EXPECT_NEAR(controller().m_angularVelocity.norm(), 0.f, 1e-5f);
}

TEST_F(ShipControllerTest, TheLevelRateRollsTheWingsBackLevel) {
	controller().levelRate = 2.f;
	hold(input::key::D, 60);
	EXPECT_LT(pose().rotation().z(), -0.3f);
	run(300);
	EXPECT_NEAR(pose().rotation().z(), 0.f, 0.02f);
	// Without the assist, the roll stays.
	controller().flightAssist = false;
	hold(input::key::D, 30);
	run(30);
	const float rolled = pose().rotation().z();
	controller().m_angularVelocity = {0.f, 0.f, 0.f};
	run(100);
	EXPECT_NEAR(pose().rotation().z(), rolled, 1e-3f);
}

TEST_F(ShipControllerTest, TheArcadeFlightYawsAboutTheWorldUpAndBoundsThePitch) {
	controller().maxPitch = 0.8f;
	controller().levelRate = 1.2f;
	// Yaw and pitch held together: the wings stay level and the nose stops at the pitch bound.
	input::Input::injectKey(input::key::Left);
	hold(input::key::Up, 300);
	input::Input::injectKey(input::key::Left);
	const auto& r = pose().rotation();
	const float upY = math::sin(r.z()) * math::sin(r.y()) * math::sin(r.x()) + math::cos(r.z()) * math::cos(r.x());
	const float noseY = -(math::sin(r.z()) * math::sin(r.y()) * math::cos(r.x()) - math::cos(r.z()) * math::sin(r.x()));
	EXPECT_NEAR(noseY, math::sin(0.8f), 0.02f);
	EXPECT_GT(upY, 0.f);
	// The wings lie flat: the body right has no vertical component.
	const float rightY = math::sin(r.z()) * math::cos(r.y());
	EXPECT_NEAR(rightY, 0.f, 0.02f);
}

TEST_F(ShipControllerTest, WithoutAngularResponseTheTurnFollowsTheKeysAtOnce) {
	controller().angularResponse = 0.f;
	// The first frame already turns at the full rate, and the release stops the turn on the next frame.
	hold(input::key::Up, 1);
	EXPECT_NEAR(controller().m_angularVelocity.x(), controller().turnRate, 1e-5f);
	EXPECT_NEAR(pose().rotation().x(), controller().turnRate * 0.01f, 1e-4f);
	run(1);
	EXPECT_NEAR(controller().m_angularVelocity.norm(), 0.f, 1e-6f);
	const float pitched = pose().rotation().x();
	run(50);
	EXPECT_NEAR(pose().rotation().x(), pitched, 1e-5f);
	// Without the assist either: no spin is kept.
	controller().flightAssist = false;
	hold(input::key::Up, 10);
	run(1);
	EXPECT_NEAR(controller().m_angularVelocity.norm(), 0.f, 1e-6f);
}

TEST_F(ShipControllerTest, WithoutMouseSteerTheCursorIsStillCapturedForTheGame) {
	controller().captureCursor = true;
	controller().mouseSteer = false;
	EXPECT_TRUE(scene.wantsCursorCapture());
	hold(input::key::Left, 20);
	EXPECT_GT(pose().rotation().y(), 0.05f);
}

TEST_F(ShipControllerTest, APersistentThrottleHoldsItsSpeedOnceReleased) {
	controller().throttleRate = 1.f;
	controller().minThrottle = -0.5f;
	controller().maxSpeed = 20.f;
	controller().thrust = 40.f;
	// Half a second of W raises the throttle to one half: the ship holds 10 m/s after the release.
	hold(input::key::W, 50);
	EXPECT_NEAR(controller().m_throttle, 0.5f, 0.02f);
	run(100);
	EXPECT_NEAR(controller().m_throttle, 0.5f, 0.02f);
	EXPECT_NEAR(controller().getSpeed(), 10.f, 0.3f);
	EXPECT_LT(controller().m_velocity.z(), -9.f);
	// S lowers it down to the reverse limit, and the ship backs at that share of its top speed.
	hold(input::key::S, 200);
	EXPECT_NEAR(controller().m_throttle, -0.5f, 1e-5f);
	run(100);
	EXPECT_NEAR(controller().m_velocity.z(), 10.f, 0.3f);
}

TEST_F(ShipControllerTest, APersistentThrottleIsReachedAtMostAtFullThrust) {
	controller().throttleRate = 100.f;
	controller().maxSpeed = 50.f;
	controller().thrust = 10.f;
	controller().assistDamping = 0.f;
	hold(input::key::W, 1);
	EXPECT_NEAR(controller().m_throttle, 1.f, 1e-5f);
	run(99);
	EXPECT_NEAR(controller().getSpeed(), 10.f, 0.2f);
}

TEST_F(ShipControllerTest, AScriptTurnTurnsTowardTheScreenDirectionOnce) {
	controller().angularResponse = 0.f;
	controller().invertPitch = true;
	controller().m_turnInput = {0.5f, 1.f};
	run(1);
	// Right and down on the screen: the nose yaws right and pitches down at those shares of the rate, never inverted.
	EXPECT_NEAR(controller().m_angularVelocity.y(), -0.5f * controller().turnRate, 1e-5f);
	EXPECT_NEAR(controller().m_angularVelocity.x(), -controller().turnRate, 1e-5f);
	EXPECT_NEAR(controller().m_turnInput.norm(), 0.f, 1e-6f);
	run(1);
	EXPECT_NEAR(controller().m_angularVelocity.norm(), 0.f, 1e-6f);
}
