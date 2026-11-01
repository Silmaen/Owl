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
