/**
 * @file PhysicFixedStep_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

class PhysicFixedStepTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override {
		if (PhysicCommand::isInitialized())
			PhysicCommand::destroy();
		core::Log::invalidate();
	}
};

auto makeFrame(const int64_t iMicroseconds) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(iMicroseconds));
	return ts;
}

void addGround(Scene& ioScene) {
	auto ground = ioScene.createEntity("ground");
	ground.getComponent<component::Transform>().transform.scale() = {40.f, 1.f, 1.f};
	ground.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
}

auto addBox(Scene& ioScene, const math::vec3f& iPosition) -> Entity {
	auto box = ioScene.createEntity("box");
	box.getComponent<component::Transform>().transform.translation() = iPosition;
	box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
	return box;
}

auto makeStack(Scene& ioScene) -> std::vector<Entity> {
	addGround(ioScene);
	std::vector<Entity> boxes;
	for (int i = 0; i < 12; ++i)
		boxes.push_back(addBox(ioScene,
							   {static_cast<float>(i % 4) * 1.1f - 2.f, 1.5f + static_cast<float>(i / 4) * 1.2f, 0.f}));
	return boxes;
}

auto readPositions(const std::vector<Entity>& iBoxes) -> std::vector<math::vec3f> {
	std::vector<math::vec3f> out;
	out.reserve(iBoxes.size());
	for (const auto& box: iBoxes) out.push_back(box.getComponent<component::Transform>().transform.translation());
	return out;
}

auto simulate(const std::vector<int64_t>& iFrames) -> std::vector<math::vec3f> {
	Scene scene;
	const auto boxes = makeStack(scene);
	PhysicCommand::init(&scene);
	for (const auto frame: iFrames) PhysicCommand::frame(makeFrame(frame));
	PhysicCommand::syncSimulatedTransforms();
	auto positions = readPositions(boxes);
	PhysicCommand::destroy();
	return positions;
}

}// namespace

TEST_F(PhysicFixedStepTest, SameStepsGiveSamePositionsWhateverTheFrameDurations) {
	// 2.0083 s in both runs: 120 fixed steps, split into regular or ragged frames.
	const std::vector<int64_t> regular(120, 16'667);
	std::vector<int64_t> ragged;
	int64_t total = 0;
	const std::array<int64_t, 7> pattern{7'000, 23'000, 16'000, 31'000, 3'000, 11'000, 25'000};
	for (size_t i = 0; total + pattern[i % pattern.size()] < 2'000'040; ++i) {
		ragged.push_back(pattern[i % pattern.size()]);
		total += ragged.back();
	}
	ragged.push_back(2'008'300 - total);
	const auto first = simulate(regular);
	const auto second = simulate(ragged);
	const auto third = simulate(ragged);
	ASSERT_EQ(first.size(), second.size());
	for (size_t i = 0; i < first.size(); ++i) {
		EXPECT_EQ(first[i], second[i]) << "box " << i;
		EXPECT_EQ(second[i], third[i]) << "box " << i;
	}
	EXPECT_LT(first.front().y(), 1.5f);
}

TEST_F(PhysicFixedStepTest, StepCountFollowsTheTickRate) {
	Scene scene;
	addBox(scene, {0.f, 5.f, 0.f});
	scene.getPhysicsSettings().tickRate = 120.f;
	PhysicCommand::init(&scene);
	EXPECT_FLOAT_EQ(PhysicCommand::getSettings().tickRate, 120.f);
	PhysicCommand::frame(makeFrame(16'667));
	EXPECT_EQ(PhysicCommand::getLastFrameStepCount(), 2u);
	PhysicCommand::frame(makeFrame(4'000));
	EXPECT_EQ(PhysicCommand::getLastFrameStepCount(), 0u);
	PhysicCommand::frame(makeFrame(4'400));
	EXPECT_EQ(PhysicCommand::getLastFrameStepCount(), 1u);
}

TEST_F(PhysicFixedStepTest, StepsPerFrameAreBoundedAndTheExcessIsDropped) {
	Scene scene;
	addBox(scene, {0.f, 5.f, 0.f});
	scene.getPhysicsSettings().maxStepsPerFrame = 5;
	PhysicCommand::init(&scene);
	PhysicCommand::frame(makeFrame(2'000'000));
	EXPECT_EQ(PhysicCommand::getLastFrameStepCount(), 5u);
	PhysicCommand::frame(makeFrame(0));
	EXPECT_EQ(PhysicCommand::getLastFrameStepCount(), 0u);
	PhysicCommand::frame(makeFrame(16'667));
	EXPECT_EQ(PhysicCommand::getLastFrameStepCount(), 1u);
}

TEST_F(PhysicFixedStepTest, InterpolationBlendsTheLastTwoSteps) {
	Scene scene;
	auto box = addBox(scene, {0.f, 0.f, 0.f});
	PhysicCommand::init(&scene);
	PhysicCommand::setGravityScale(box, 0.f);
	PhysicCommand::setVelocity(box, {6.f, 0.f});
	const auto& transform = box.getComponent<component::Transform>().transform;
	constexpr float step = 1.f / 60.f;

	PhysicCommand::frame(makeFrame(25'000));
	ASSERT_EQ(PhysicCommand::getLastFrameStepCount(), 1u);
	const float alpha = PhysicCommand::getInterpolationAlpha();
	EXPECT_NEAR(alpha, 0.5f, 1e-3f);
	EXPECT_NEAR(transform.translation().x(), 6.f * step * alpha, 1e-4f);

	PhysicCommand::frame(makeFrame(4'000));
	EXPECT_EQ(PhysicCommand::getLastFrameStepCount(), 0u);
	EXPECT_GT(PhysicCommand::getInterpolationAlpha(), alpha);
	EXPECT_NEAR(transform.translation().x(), 6.f * step * PhysicCommand::getInterpolationAlpha(), 1e-4f);

	PhysicCommand::syncSimulatedTransforms();
	EXPECT_NEAR(transform.translation().x(), 6.f * step, 1e-4f);
}

TEST_F(PhysicFixedStepTest, TeleportIsNotInterpolated) {
	Scene scene;
	auto box = addBox(scene, {0.f, 0.f, 0.f});
	PhysicCommand::init(&scene);
	PhysicCommand::setGravityScale(box, 0.f);
	PhysicCommand::frame(makeFrame(25'000));
	PhysicCommand::setTransform(box, {10.f, 3.f}, 0.f);
	PhysicCommand::frame(makeFrame(1'000));
	ASSERT_EQ(PhysicCommand::getLastFrameStepCount(), 0u);
	const auto& transform = box.getComponent<component::Transform>().transform;
	EXPECT_FLOAT_EQ(transform.translation().x(), 10.f);
	EXPECT_FLOAT_EQ(transform.translation().y(), 3.f);
}

TEST_F(PhysicFixedStepTest, WithoutInterpolationTransformsShowTheLastStep) {
	Scene scene;
	auto box = addBox(scene, {0.f, 0.f, 0.f});
	scene.getPhysicsSettings().interpolate = false;
	PhysicCommand::init(&scene);
	PhysicCommand::setGravityScale(box, 0.f);
	PhysicCommand::setVelocity(box, {6.f, 0.f});
	PhysicCommand::frame(makeFrame(25'000));
	EXPECT_FLOAT_EQ(PhysicCommand::getInterpolationAlpha(), 1.f);
	EXPECT_NEAR(box.getComponent<component::Transform>().transform.translation().x(), 6.f / 60.f, 1e-4f);
}

TEST_F(PhysicFixedStepTest, CollisionReportedOnceInAFrameOfManySteps) {
	Scene scene;
	for (const float x: {-2.f, 2.f}) {
		auto wall = scene.createEntity("wall");
		wall.getComponent<component::Transform>().transform.translation() = {x, 0.f, 0.f};
		wall.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
	}
	auto ball = addBox(scene, {0.f, 0.f, 0.f});
	auto& body = ball.getComponent<component::PhysicBody>().body;
	body.restitution = 1.f;
	body.friction = 0.f;
	body.fixedRotation = true;
	scene.getPhysicsSettings().maxStepsPerFrame = 64;
	PhysicCommand::init(&scene);
	PhysicCommand::setGravityScale(ball, 0.f);
	PhysicCommand::setVelocity(ball, {10.f, 0.f});
	// The ball crosses the 2 m gap every 0.2 s: each wall is hit about five times in a one-second frame.
	for (int i = 0; i < 3; ++i) {
		PhysicCommand::frame(makeFrame(1'000'000));
		ASSERT_EQ(PhysicCommand::getLastFrameStepCount(), 60u);
		const auto events = PhysicCommand::takeCollisionEvents();
		ASSERT_EQ(events.size(), 2u) << "frame " << i;
		EXPECT_NE(events[0].entityA == ball.getUUID() ? events[0].entityB : events[0].entityA,
				  events[1].entityA == ball.getUUID() ? events[1].entityB : events[1].entityA);
	}
	EXPECT_GT(std::abs(PhysicCommand::getVelocity(ball).x()), 9.f);
}

TEST(PhysicsSettings, ClampedKeepsFieldsInRange) {
	PhysicsSettings settings;
	settings.tickRate = 0.f;
	settings.maxStepsPerFrame = 0;
	settings.solverSubSteps = 1000;
	auto clamped = settings.clamped();
	EXPECT_FLOAT_EQ(clamped.tickRate, PhysicsSettings::minTickRate);
	EXPECT_EQ(clamped.maxStepsPerFrame, 1u);
	EXPECT_EQ(clamped.solverSubSteps, uint32_t{PhysicsSettings::maxSolverSubSteps});
	settings.tickRate = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FLOAT_EQ(settings.clamped().tickRate, PhysicsSettings::defaultTickRate);
	EXPECT_NEAR(PhysicsSettings{}.getStepSeconds(), 1.0 / 60.0, 1e-12);
}

TEST(PhysicsSettings, SceneRoundTripAndDefaultOmitted) {
	core::Log::init(core::Log::Level::Off);
	const auto scene = mkShared<Scene>();
	EXPECT_EQ(SceneSerializer(scene).serializeToString().find("Physics:"), std::string::npos);
	scene->getPhysicsSettings().tickRate = 90.f;
	scene->getPhysicsSettings().maxStepsPerFrame = 3;
	scene->getPhysicsSettings().solverSubSteps = 6;
	scene->getPhysicsSettings().interpolate = false;
	const std::string yaml = SceneSerializer(scene).serializeToString();
	EXPECT_NE(yaml.find("Physics:"), std::string::npos);
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>(yaml.begin(), yaml.end()), "mem"));
	EXPECT_EQ(loaded->getPhysicsSettings(), scene->getPhysicsSettings());
	EXPECT_EQ(Scene::copy(scene)->getPhysicsSettings(), scene->getPhysicsSettings());
	core::Log::invalidate();
}
