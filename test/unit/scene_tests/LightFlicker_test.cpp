/**
 * @file LightFlicker_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class LightFlickerTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(16));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto makeLight(Scene& ioScene, const float iSeed) -> Entity {
		auto entity = ioScene.createEntity("Light");
		auto& light = entity.addComponent<component::Light>();
		light.type = component::Light::Type::Point;
		light.intensity = 1000.f;
		light.color = {1.f, 0.5f, 0.2f};
		auto& flicker = entity.addComponent<component::LightFlicker>();
		flicker.seed = iSeed;
		flicker.amplitude = 0.3f;
		flicker.colorShift = {0.f, 0.1f, 0.f};
		flicker.positionJitter = {0.1f, 0.f, 0.f};
		return entity;
	}
};

}// namespace

TEST_F(LightFlickerTest, IntensityStaysWithinTheAmplitudeAroundTheAuthoredValue) {
	auto scene = mkShared<Scene>();
	const auto entity = makeLight(*scene, 3.f);
	scene->onStartRuntime();
	float lowest = 1e9f;
	float highest = -1e9f;
	for (int frame = 0; frame < 300; ++frame) {
		run(*scene, 1);
		const float intensity = entity.getComponent<component::Light>().intensity;
		lowest = std::min(lowest, intensity);
		highest = std::max(highest, intensity);
	}
	EXPECT_LT(lowest, 990.f);
	EXPECT_GT(highest, 1010.f);
	EXPECT_GE(lowest, 1000.f * 0.2f);
	EXPECT_LE(highest, 1000.f * 1.45f);
	scene->onEndRuntime();
}

TEST_F(LightFlickerTest, TheSameSeedAndTimeGiveTheSameLightAndAnotherSeedDoesNot) {
	auto first = mkShared<Scene>();
	auto second = mkShared<Scene>();
	auto other = mkShared<Scene>();
	const auto a = makeLight(*first, 5.f);
	const auto b = makeLight(*second, 5.f);
	const auto c = makeLight(*other, 6.f);
	for (const auto& scene: {first, second, other}) scene->onStartRuntime();
	for (const auto& scene: {first, second, other}) run(*scene, 40);
	EXPECT_FLOAT_EQ(a.getComponent<component::Light>().intensity, b.getComponent<component::Light>().intensity);
	EXPECT_FLOAT_EQ(a.getComponent<component::Transform>().transform.translation().x(),
					b.getComponent<component::Transform>().transform.translation().x());
	EXPECT_NE(a.getComponent<component::Light>().intensity, c.getComponent<component::Light>().intensity);
}

TEST_F(LightFlickerTest, ADisabledFlickerAndThePlayCopyLeaveTheAuthoredLight) {
	auto scene = mkShared<Scene>();
	const auto entity = makeLight(*scene, 3.f);
	entity.getComponent<component::LightFlicker>().enabled = false;
	scene->onStartRuntime();
	run(*scene, 20);
	EXPECT_FLOAT_EQ(entity.getComponent<component::Light>().intensity, 1000.f);
	entity.getComponent<component::LightFlicker>().enabled = true;
	auto copy = Scene::copy(scene);
	copy->onStartRuntime();
	run(*copy, 20);
	EXPECT_FLOAT_EQ(entity.getComponent<component::Light>().intensity, 1000.f);
}

TEST_F(LightFlickerTest, ASpriteWithoutLightIsTintedAndAFollowerBreathesWithTheLight) {
	auto scene = mkShared<Scene>();
	const auto light = makeLight(*scene, 3.f);
	auto sprite = scene->createEntity("Torch");
	sprite.addComponent<component::SpriteRenderer>().color = {1.f, 0.7f, 0.4f, 1.f};
	auto& torch = sprite.addComponent<component::LightFlicker>();
	torch.style = component::LightFlicker::Style::Pulse;
	torch.colorShift = {0.f, 0.3f, 0.5f};
	auto flame = scene->createEntity("Flame");
	auto& follower = flame.addComponent<component::LightFlicker>();
	follower.follow = light.getUUID();
	follower.scaleResponse = {0.f, 1.f, 0.f};
	scene->onStartRuntime();
	bool tinted = false;
	bool scaled = false;
	for (int frame = 0; frame < 120; ++frame) {
		run(*scene, 1);
		tinted = tinted || std::abs(sprite.getComponent<component::SpriteRenderer>().color.g() - 0.7f) > 1e-4f;
		scaled = scaled || std::abs(flame.getComponent<component::Transform>().transform.scale().y() - 1.f) > 1e-4f;
		EXPECT_FLOAT_EQ(flame.getComponent<component::Transform>().transform.scale().x(), 1.f);
	}
	EXPECT_TRUE(tinted);
	EXPECT_TRUE(scaled);
	EXPECT_FLOAT_EQ(sprite.getComponent<component::SpriteRenderer>().color.r(), 1.f);
}
