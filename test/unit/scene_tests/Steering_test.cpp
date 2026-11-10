/**
 * @file Steering_test.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/ai/Steering.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class SteeringTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(50));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto translationOf(const Entity& iEntity) -> math::vec3 {
		return iEntity.getComponent<component::Transform>().transform.translation();
	}

	static auto addAgent(Scene& ioScene, const std::string& iName, const math::vec3& iPosition) -> Entity {
		auto entity = ioScene.createEntity(iName);
		entity.getComponent<component::Transform>().transform.translation() = iPosition;
		entity.addComponent<component::SteeringAgent>();
		return entity;
	}

	// A flock of twelve fish in a ring, in open space.
	static auto makeShoal(Scene& ioScene) -> std::vector<Entity> {
		std::vector<Entity> fish;
		for (int index = 0; index < 12; ++index) {
			const float angle = static_cast<float>(index) * 0.5236f;
			auto entity =
					addAgent(ioScene, std::format("Fish{}", index),
							 {3.f * std::cos(angle), 0.3f * static_cast<float>(index % 3), 3.f * std::sin(angle)});
			auto& agent = entity.getComponent<component::SteeringAgent>();
			agent.plane = ai::SteeringPlane::Space;
			agent.flock = "shoal";
			agent.maxSpeed = 3.f;
			agent.radius = 0.3f;
			agent.weights = {.separation = 2.f, .alignment = 1.f, .cohesion = 1.f, .avoidance = 1.f};
			agent.weights.wander = 0.5f;
			fish.push_back(entity);
		}
		return fish;
	}
};

}// namespace

TEST_F(SteeringTest, SeekAndArriveHeadToTheTarget) {
	const ai::SteeringBody agent{.position = {0.f, 0.f, 0.f}, .velocity = {0.f, 0.f, 0.f}, .radius = 0.5f};
	const auto seek = ai::seek(agent, {10.f, 0.f, 0.f}, 4.f);
	EXPECT_FLOAT_EQ(seek.x(), 4.f);
	EXPECT_FLOAT_EQ(seek.y(), 0.f);
	// Inside the slowing radius, arrive asks for less than full speed.
	const auto arrive = ai::arrive(agent, {1.f, 0.f, 0.f}, 4.f, 2.f);
	EXPECT_FLOAT_EQ(arrive.x(), 2.f);
	// On the target, arrive brakes.
	const ai::SteeringBody moving{.position = {1.f, 0.f, 0.f}, .velocity = {1.f, 0.f, 0.f}, .radius = 0.5f};
	EXPECT_FLOAT_EQ(ai::arrive(moving, {1.f, 0.f, 0.f}, 4.f, 2.f).x(), -1.f);
}

TEST_F(SteeringTest, FleeAndEvadeOnlyInsideThePanicDistance) {
	const ai::SteeringBody agent{.position = {0.f, 0.f, 0.f}, .velocity = {0.f, 0.f, 0.f}, .radius = 0.5f};
	EXPECT_FLOAT_EQ(ai::flee(agent, {2.f, 0.f, 0.f}, 3.f, 5.f).x(), -3.f);
	EXPECT_FLOAT_EQ(ai::flee(agent, {20.f, 0.f, 0.f}, 3.f, 5.f).norm(), 0.f);
	EXPECT_FLOAT_EQ(ai::flee(agent, {20.f, 0.f, 0.f}, 3.f, 0.f).x(), -3.f);
	const ai::SteeringBody threat{.position = {2.f, 0.f, 0.f}, .velocity = {0.f, 0.f, 1.f}, .radius = 0.5f};
	EXPECT_LT(ai::evade(agent, threat, 3.f, 1.f, 5.f).x(), 0.f);
	EXPECT_FLOAT_EQ(ai::evade(agent, threat, 3.f, 1.f, 1.f).norm(), 0.f);
}

TEST_F(SteeringTest, PursueAimsWhereTheQuarryWillBe) {
	const ai::SteeringBody agent{.position = {0.f, 0.f, 0.f}, .velocity = {2.f, 0.f, 0.f}, .radius = 0.5f};
	const ai::SteeringBody quarry{.position = {4.f, 0.f, 0.f}, .velocity = {0.f, 0.f, 2.f}, .radius = 0.5f};
	const auto chase = ai::pursue(agent, quarry, 4.f, 1.f);
	EXPECT_GT(chase.z(), 0.5f);
}

TEST_F(SteeringTest, AvoidancePushesSidewaysOnlyWhenOnACollisionCourse) {
	const ai::SteeringBody agent{.position = {0.f, 0.f, 0.f}, .velocity = {2.f, 0.f, 0.f}, .radius = 0.5f};
	const std::vector<ai::SteeringBody> ahead{
			{.position = {3.f, 0.f, 0.2f}, .velocity = {-2.f, 0.f, 0.f}, .radius = 0.5f}};
	const auto push = ai::avoidVolumes(agent, ahead, 1.5f, 2.f);
	EXPECT_LT(push.z(), -0.5f);
	const std::vector<ai::SteeringBody> leaving{
			{.position = {3.f, 0.f, 0.f}, .velocity = {4.f, 0.f, 0.f}, .radius = 0.5f}};
	EXPECT_FLOAT_EQ(ai::avoidVolumes(agent, leaving, 1.5f, 2.f).norm(), 0.f);
	const std::vector<ai::SteeringBody> touching{
			{.position = {0.5f, 0.f, 0.f}, .velocity = {0.f, 0.f, 0.f}, .radius = 0.5f}};
	EXPECT_LT(ai::avoidVolumes(agent, touching, 1.5f, 2.f).x(), 0.f);
}

TEST_F(SteeringTest, FlockingRulesPullPushAndAlign) {
	const ai::SteeringBody agent{.position = {0.f, 0.f, 0.f}, .velocity = {0.f, 0.f, 0.f}, .radius = 0.5f};
	const std::vector<ai::SteeringBody> flock{
			{.position = {1.2f, 0.f, 0.f}, .velocity = {0.f, 1.f, 0.f}, .radius = 0.5f},
			{.position = {5.f, 0.f, 0.f}, .velocity = {0.f, 1.f, 0.f}, .radius = 0.5f}};
	EXPECT_LT(ai::separation(agent, flock, 1.f, 2.f).x(), 0.f);
	EXPECT_GT(ai::cohesion(agent, flock, 2.f).x(), 0.f);
	EXPECT_FLOAT_EQ(ai::alignment(agent, flock, 2.f).y(), 2.f);
	EXPECT_FLOAT_EQ(ai::cohesion(agent, {}, 2.f).norm(), 0.f);
}

TEST_F(SteeringTest, TheProjectionKeepsTheAgentOnItsPlane) {
	const math::vec3 vector{1.f, 2.f, 3.f};
	EXPECT_EQ(ai::projectOnPlane(vector, ai::SteeringPlane::Plane2D), math::vec3(1.f, 2.f, 0.f));
	EXPECT_EQ(ai::projectOnPlane(vector, ai::SteeringPlane::Ground), math::vec3(1.f, 0.f, 3.f));
	EXPECT_EQ(ai::projectOnPlane(vector, ai::SteeringPlane::Space), vector);
	EXPECT_NEAR(ai::truncate({3.f, 4.f, 0.f}, 1.f).norm(), 1.f, 1e-6f);
}

TEST_F(SteeringTest, AnAgentArrivesOnItsTargetPointAndStops) {
	auto scene = mkShared<Scene>();
	auto walker = addAgent(*scene, "Walker", {0.f, 1.f, 0.f});
	auto& agent = walker.getComponent<component::SteeringAgent>();
	agent.weights.arrive = 1.f;
	agent.targetPoint = {6.f, 5.f, 2.f};
	scene->onStartRuntime();
	run(*scene, 120);
	EXPECT_NEAR(translationOf(walker).x(), 6.f, 0.1f);
	EXPECT_NEAR(translationOf(walker).z(), 2.f, 0.1f);
	// On the ground, the height never changes.
	EXPECT_FLOAT_EQ(translationOf(walker).y(), 1.f);
	EXPECT_LT(walker.getComponent<component::SteeringAgent>().m_velocity.norm(), 0.1f);
	scene->onEndRuntime();
}

TEST_F(SteeringTest, A2DAgentStaysOnItsPlaneAndTurnsAroundZ) {
	auto scene = mkShared<Scene>();
	auto bee = addAgent(*scene, "Bee", {0.f, 0.f, 0.5f});
	auto& agent = bee.getComponent<component::SteeringAgent>();
	agent.plane = ai::SteeringPlane::Plane2D;
	agent.weights.seek = 1.f;
	agent.targetPoint = {0.f, 10.f, 4.f};
	scene->onStartRuntime();
	run(*scene, 10);
	EXPECT_GT(translationOf(bee).y(), 1.f);
	EXPECT_FLOAT_EQ(translationOf(bee).z(), 0.5f);
	EXPECT_NEAR(bee.getComponent<component::Transform>().transform.rotation().z(), 1.5708f, 0.05f);
	scene->onEndRuntime();
}

TEST_F(SteeringTest, APursuerCatchesAMovingTarget) {
	auto scene = mkShared<Scene>();
	auto prey = addAgent(*scene, "Prey", {4.f, 0.f, 0.f});
	auto& running = prey.getComponent<component::SteeringAgent>();
	running.maxSpeed = 2.f;
	running.weights.seek = 1.f;
	running.targetPoint = {4.f, 0.f, 100.f};
	auto hunter = addAgent(*scene, "Hunter", {0.f, 0.f, 0.f});
	auto& chasing = hunter.getComponent<component::SteeringAgent>();
	chasing.maxSpeed = 5.f;
	chasing.weights = {.pursue = 1.f, .avoidance = 0.f};
	chasing.target = prey.getUUID();
	scene->onStartRuntime();
	float closest = 100.f;
	for (int frame = 0; frame < 80; ++frame) {
		run(*scene, 1);
		closest = std::min(closest, (translationOf(prey) - translationOf(hunter)).norm());
	}
	EXPECT_LT(closest, 1.f);
	scene->onEndRuntime();
}

TEST_F(SteeringTest, AShoalStaysTogetherWithoutOverlapping) {
	auto scene = mkShared<Scene>();
	const auto fish = makeShoal(*scene);
	scene->onStartRuntime();
	run(*scene, 100);
	math::vec3 centre{0.f, 0.f, 0.f};
	for (const auto& member: fish) centre += translationOf(member);
	centre *= 1.f / static_cast<float>(fish.size());
	for (size_t first = 0; first < fish.size(); ++first) {
		EXPECT_LT((translationOf(fish[first]) - centre).norm(), 8.f);
		for (size_t second = first + 1; second < fish.size(); ++second)
			EXPECT_GT((translationOf(fish[first]) - translationOf(fish[second])).norm(), 0.3f);
	}
	// The members head the same way.
	const math::vec3 heading = fish.front().getComponent<component::SteeringAgent>().m_velocity.normalized();
	int aligned = 0;
	for (const auto& member: fish)
		if (member.getComponent<component::SteeringAgent>().m_velocity.normalized() * heading > 0.f)
			++aligned;
	EXPECT_GE(aligned, 9);
	scene->onEndRuntime();
}

TEST_F(SteeringTest, TheSameShoalSwimsTheSameWay) {
	auto authored = mkShared<Scene>();
	std::ignore = makeShoal(*authored);
	authored->setRandomSeed(42);
	const auto text = SceneSerializer(authored).serializeToString();
	const auto swim = [&text]() -> std::vector<math::vec3> {
		auto scene = mkShared<Scene>();
		EXPECT_TRUE(SceneSerializer(scene).deserializeFromBuffer({text.begin(), text.end()}));
		scene->onStartRuntime();
		run(*scene, 40);
		std::vector<math::vec3> positions;
		for (const auto view = scene->registry.view<component::SteeringAgent, component::ID>();
			 const auto handle: view) {
			const Entity entity{handle, scene.get()};
			positions.push_back(translationOf(entity));
		}
		scene->onEndRuntime();
		return positions;
	};
	const auto first = swim();
	EXPECT_EQ(first.size(), 12);
	EXPECT_EQ(first, swim());
}

TEST_F(SteeringTest, AFollowerHoldsItsSlotInTheFormation) {
	auto scene = mkShared<Scene>();
	auto captain = addAgent(*scene, "Captain", {0.f, 0.f, 0.f});
	auto& lead = captain.getComponent<component::SteeringAgent>();
	lead.maxSpeed = 2.f;
	lead.weights.seek = 1.f;
	lead.targetPoint = {0.f, 0.f, 100.f};
	auto soldier = addAgent(*scene, "Soldier", {-3.f, 0.f, -3.f});
	auto& follower = soldier.getComponent<component::SteeringAgent>();
	follower.maxSpeed = 4.f;
	follower.weights = {.avoidance = 0.f, .formation = 1.f};
	follower.leader = captain.getUUID();
	follower.slot = {1.5f, 0.f, -2.f};
	scene->onStartRuntime();
	run(*scene, 120);
	// The captain faces +z, so the slot is to its right (+x) and behind it (-z).
	const math::vec3 offset = translationOf(soldier) - translationOf(captain);
	EXPECT_NEAR(offset.x(), 1.5f, 0.3f);
	EXPECT_NEAR(offset.z(), -2.f, 0.5f);
	scene->onEndRuntime();
}

TEST_F(SteeringTest, ADisabledAgentStandsStill) {
	auto scene = mkShared<Scene>();
	auto walker = addAgent(*scene, "Walker", {0.f, 0.f, 0.f});
	auto& agent = walker.getComponent<component::SteeringAgent>();
	agent.weights.seek = 1.f;
	agent.targetPoint = {5.f, 0.f, 0.f};
	agent.enabled = false;
	scene->onStartRuntime();
	run(*scene, 10);
	EXPECT_FLOAT_EQ(translationOf(walker).x(), 0.f);
	scene->onEndRuntime();
}

TEST_F(SteeringTest, TheComponentSurvivesASaveAndALoad) {
	auto scene = mkShared<Scene>();
	auto leader = scene->createEntity("Leader");
	auto bird = addAgent(*scene, "Bird", {0.f, 0.f, 0.f});
	auto& agent = bird.getComponent<component::SteeringAgent>();
	agent.enabled = false;
	agent.plane = ai::SteeringPlane::Space;
	agent.maxSpeed = 7.f;
	agent.maxAcceleration = 3.f;
	agent.radius = 0.2f;
	agent.turn = false;
	agent.target = leader.getUUID();
	agent.targetPoint = {1.f, 2.f, 3.f};
	agent.weights = {.seek = 0.5f, .wander = 2.f, .cohesion = 1.5f, .avoidance = 0.f, .formation = 3.f};
	agent.slowRadius = 4.f;
	agent.panicDistance = 0.f;
	agent.prediction = 2.f;
	agent.wanderSettings = {.radius = 3.f, .distance = 5.f, .jitter = 0.5f};
	agent.flock = "starlings";
	agent.neighbourRadius = 6.f;
	agent.separationDistance = 0.4f;
	agent.lookAhead = 3.f;
	agent.leader = leader.getUUID();
	agent.slot = {2.f, 1.f, -1.f};

	const auto text = SceneSerializer(scene).serializeToString();
	auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer({text.begin(), text.end()}));
	const auto& read = loaded->findEntityByUUID(bird.getUUID()).getComponent<component::SteeringAgent>();
	EXPECT_FALSE(read.enabled);
	EXPECT_EQ(read.plane, ai::SteeringPlane::Space);
	EXPECT_FLOAT_EQ(read.maxSpeed, 7.f);
	EXPECT_FLOAT_EQ(read.maxAcceleration, 3.f);
	EXPECT_FLOAT_EQ(read.radius, 0.2f);
	EXPECT_FALSE(read.turn);
	EXPECT_EQ(read.target, leader.getUUID());
	EXPECT_EQ(read.targetPoint, agent.targetPoint);
	EXPECT_EQ(read.weights, agent.weights);
	EXPECT_FLOAT_EQ(read.slowRadius, 4.f);
	EXPECT_FLOAT_EQ(read.panicDistance, 0.f);
	EXPECT_FLOAT_EQ(read.prediction, 2.f);
	EXPECT_FLOAT_EQ(read.wanderSettings.radius, 3.f);
	EXPECT_FLOAT_EQ(read.wanderSettings.distance, 5.f);
	EXPECT_FLOAT_EQ(read.wanderSettings.jitter, 0.5f);
	EXPECT_EQ(read.flock, "starlings");
	EXPECT_FLOAT_EQ(read.neighbourRadius, 6.f);
	EXPECT_FLOAT_EQ(read.separationDistance, 0.4f);
	EXPECT_FLOAT_EQ(read.lookAhead, 3.f);
	EXPECT_EQ(read.leader, leader.getUUID());
	EXPECT_EQ(read.slot, agent.slot);
}

TEST_F(SteeringTest, AWeightIsFoundByItsName) {
	component::SteeringWeights weights;
	ASSERT_NE(weights.find("cohesion"), nullptr);
	*weights.find("cohesion") = 2.f;
	EXPECT_FLOAT_EQ(weights.cohesion, 2.f);
	EXPECT_EQ(weights.find("dance"), nullptr);
}
