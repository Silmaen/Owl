/**
 * @file FairPlay_test.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <input/Input.h>
#include <math/trigonometry.h>
#include <physics/PhysicCommand.h>
#include <renderer/LocalShadows.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneCamera.h>
#include <scene/component/components.h>
#include <window/Window.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

void run(Scene& ioScene, const int iFrames, const int iMilliseconds) {
	core::Timestep step;
	for (int frame = 0; frame < iFrames; ++frame) {
		step.forceUpdate(std::chrono::milliseconds(iMilliseconds));
		ioScene.onUpdateRuntime(step, false);
	}
}

class FairPlayTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		input::Input::init(window::Type::Null);
	}
	void TearDown() override {
		input::Input::invalidate();
		core::Log::invalidate();
	}

	// Velocity of a player holding D for one second, cut in frames of the given length.
	static auto playerVelocity(const int iFrames, const int iMilliseconds) -> math::vec2f {
		Scene scene;
		scene.getPhysicsSettings().gravity = {0.f, 0.f, 0.f};
		auto player = scene.createEntity("Player");
		player.addComponent<component::PhysicBody>();
		player.addComponent<component::Player>().primary = true;
		scene.onStartRuntime();
		input::Input::injectKey(input::key::D);
		run(scene, iFrames, iMilliseconds);
		input::Input::injectKey(input::key::D);
		const auto velocity = physics::PhysicCommand::getVelocity(player);
		scene.onEndRuntime();
		return velocity;
	}

	// Height left after one second of fall of a first person controller.
	static auto fallHeight(const int iFrames, const int iMilliseconds) -> float {
		Scene scene;
		auto walker = scene.createEntity("Walker");
		walker.getComponent<component::Transform>().transform.translation() = {0.f, 100.f, 0.f};
		walker.addComponent<component::FirstPersonController>().captureCursor = false;
		scene.onStartRuntime();
		run(scene, iFrames, iMilliseconds);
		const float height = walker.getComponent<component::Transform>().transform.translation().y();
		scene.onEndRuntime();
		return height;
	}
};

}// namespace

TEST_F(FairPlayTest, aBoundedCameraShowsTheWidthOfItsRatioOnAWiderScreen) {
	SceneCamera camera;
	camera.setPerspective(math::radians(60.f), 0.1f, 100.f);
	camera.setMaxAspectRatio(16.f / 9.f);
	const float boundedHorizontal = 2.f * std::atan(std::tan(math::radians(30.f)) * 16.f / 9.f);
	camera.setViewportSize({3440, 1440});
	EXPECT_NEAR(camera.getEffectiveHorizontalFov(), boundedHorizontal, 1e-4f);
	EXPECT_LT(camera.getEffectiveVerticalFov(), math::radians(60.f));
	camera.setViewportSize({1600, 1200});
	EXPECT_NEAR(camera.getEffectiveVerticalFov(), math::radians(60.f), 1e-5f);
	EXPECT_LT(camera.getEffectiveHorizontalFov(), boundedHorizontal);
	camera.setMaxAspectRatio(0.f);
	camera.setViewportSize({3440, 1440});
	EXPECT_GT(camera.getEffectiveHorizontalFov(), boundedHorizontal);
	EXPECT_FLOAT_EQ(camera.getMaxAspectRatio(), 0.f);
	camera.setMaxAspectRatio(-2.f);
	EXPECT_FLOAT_EQ(camera.getMaxAspectRatio(), 0.f);
}

TEST_F(FairPlayTest, aBoundedOrthographicCameraKeepsItsVisibleWidth) {
	SceneCamera camera;
	camera.setOrthographic(10.f, -1.f, 1.f);
	camera.setMaxAspectRatio(16.f / 9.f);
	camera.setViewportSize({3440, 1440});
	const float aspect = 3440.f / 1440.f;
	EXPECT_NEAR(camera.getEffectiveOrthographicSize() * aspect, 10.f * 16.f / 9.f, 1e-4f);
	camera.setViewportSize({1920, 1080});
	EXPECT_NEAR(camera.getEffectiveOrthographicSize(), 10.f, 1e-4f);
}

TEST_F(FairPlayTest, gameplayLightsKeepTheirShadowsFirstWithinTheCommonSlots) {
	std::vector<renderer::ShadowCandidate> candidates;
	for (uint32_t index = 0; index < 8; ++index)
		candidates.push_back({.lightIndex = index, .distance = static_cast<float>(index), .intensity = 1.f});
	// The two farthest lights are tagged gameplay: an OpenGL budget of 4 must still keep them.
	candidates[6].gameplay = true;
	candidates[7].gameplay = true;
	renderer::rankShadowCandidates(candidates, 4);
	ASSERT_EQ(candidates.size(), 4u);
	EXPECT_EQ(candidates[0].lightIndex, 6u);
	EXPECT_EQ(candidates[1].lightIndex, 7u);
	EXPECT_EQ(candidates[2].lightIndex, 0u);
	EXPECT_EQ(candidates[3].lightIndex, 1u);
}

TEST_F(FairPlayTest, gameplayLightsBeyondTheCommonSlotsCompeteAsTheOthers) {
	std::vector<renderer::ShadowCandidate> candidates;
	for (uint32_t index = 0; index < 8; ++index)
		candidates.push_back(
				{.lightIndex = index, .distance = static_cast<float>(8 - index), .intensity = 1.f, .gameplay = true});
	candidates.push_back({.lightIndex = 8, .distance = 0.f, .intensity = 1.f});
	renderer::rankShadowCandidates(candidates, 16);
	ASSERT_EQ(candidates.size(), 9u);
	// The nearest renderer::g_gameplayShadowSlots gameplay lights first, then the rest by distance.
	EXPECT_EQ(candidates[0].lightIndex, 7u);
	EXPECT_EQ(candidates[renderer::g_gameplayShadowSlots - 1].lightIndex, 8u - renderer::g_gameplayShadowSlots);
	EXPECT_EQ(candidates[renderer::g_gameplayShadowSlots].lightIndex, 8u);
}

TEST_F(FairPlayTest, thePlayerIsPushedPerTickWhateverTheFrameRate) {
	const auto slow = playerVelocity(25, 40);
	const auto fast = playerVelocity(250, 4);
	EXPECT_GT(slow.x(), 0.f);
	EXPECT_FLOAT_EQ(slow.x(), fast.x());
}

TEST_F(FairPlayTest, aFirstPersonFallDoesNotDependOnTheFrameRate) {
	const float slow = fallHeight(20, 50);
	const float fast = fallHeight(250, 4);
	EXPECT_LT(slow, 100.f);
	EXPECT_NEAR(slow, fast, 1e-3f);
}
