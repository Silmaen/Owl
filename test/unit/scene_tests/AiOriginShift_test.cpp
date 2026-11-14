/**
 * @file AiOriginShift_test.cpp
 * @author Silmaen
 * @date 12/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/ai/BehaviourTree.h>
#include <scene/ai/BehaviourTreeInstance.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>

using namespace owl;
using namespace owl::scene;

namespace {

constexpr math::vec3d g_shift{100.0, 20.0, -50.0};
const math::vec3 g_shiftF{100.f, 20.f, -50.f};

class AiOriginShiftTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

void expectNear(const math::vec3& iActual, const math::vec3& iExpected, const float iTolerance = 1e-4f) {
	EXPECT_NEAR(iActual.x(), iExpected.x(), iTolerance);
	EXPECT_NEAR(iActual.y(), iExpected.y(), iTolerance);
	EXPECT_NEAR(iActual.z(), iExpected.z(), iTolerance);
}

// Builds a tree node by node.
class TreeBuilder {
public:
	auto add(const std::string& iType, std::map<std::string, std::string> iValues = {},
			 const std::optional<core::UUID> iParent = std::nullopt) -> core::UUID {
		const core::UUID id{m_next++};
		m_tree.nodes.push_back({.id = id,
								.type = iType,
								.position = {0.f, static_cast<float>(m_next) * 10.f},
								.values = std::move(iValues),
								.comment = {}});
		if (iParent.has_value())
			m_tree.links.push_back({.parent = *iParent, .child = id});
		return id;
	}

	void key(const std::string& iName, ai::BlackboardValue iValue) {
		m_tree.blackboard.push_back({.name = iName, .value = std::move(iValue), .description = {}});
	}

	auto tree() -> ai::BehaviourTree& { return m_tree; }

private:
	ai::BehaviourTree m_tree;
	uint64_t m_next = 1;
};

auto vectorOf(const ai::BehaviourTreeInstance& iInstance, const std::string& iKey) -> math::vec3 {
	const auto* value = iInstance.getValue(iKey);
	if (value == nullptr || !std::holds_alternative<math::vec3>(*value))
		return {0.f, 0.f, 0.f};
	return std::get<math::vec3>(*value);
}

}// namespace

TEST_F(AiOriginShiftTest, NavigationPointsFollowTheShift) {
	Scene scene;
	auto volume = scene.createEntity("volume");
	auto& mesh = volume.addComponent<component::NavMeshVolume>();
	mesh.vertices = {{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {0.f, 0.f, 10.f}};
	mesh.indices = {0, 1, 2};
	auto room = scene.createEntity("room");
	auto& walkMesh = room.addComponent<component::NavMeshVolume>();
	walkMesh.sources = component::NavMeshVolume::Sources::WalkAreas;
	walkMesh.vertices = {{1.f, 0.f, 2.f}};
	auto parent = scene.createEntity("parent");
	auto agent = scene.createEntity("agent");
	scene.setParent(agent, parent);
	auto& navAgent = agent.addComponent<component::NavAgent>();
	navAgent.goTo({4.f, 0.f, 6.f});
	navAgent.m_path = {{.position = {1.f, 0.f, 1.f}, .link = -1}, {.position = {4.f, 0.f, 6.f}, .link = -1}};
	auto& obstacle = scene.createEntity("crate").addComponent<component::NavObstacle>();
	obstacle.m_lastPosition = {3.f, 0.f, 3.f};
	obstacle.m_started = true;
	auto& walker = scene.createEntity("walker").addComponent<component::WalkAgent>();
	walker.m_path = {{2.f, 3.f}, {5.f, 7.f}};

	scene.shiftWorldOrigin(g_shift);

	expectNear(mesh.vertices[1], math::vec3{10.f, 0.f, 0.f} - g_shiftF);
	// A mesh of walk areas holds the plane (x, y) as (x, z).
	expectNear(walkMesh.vertices[0], {1.f - g_shiftF.x(), 0.f, 2.f - g_shiftF.y()});
	expectNear(navAgent.m_target, math::vec3{4.f, 0.f, 6.f} - g_shiftF);
	expectNear(navAgent.m_path[0].position, math::vec3{1.f, 0.f, 1.f} - g_shiftF);
	expectNear(navAgent.m_path[1].position, math::vec3{4.f, 0.f, 6.f} - g_shiftF);
	expectNear(obstacle.m_lastPosition, math::vec3{3.f, 0.f, 3.f} - g_shiftF);
	EXPECT_NEAR(walker.m_path[1].x(), 5.f - g_shiftF.x(), 1e-4f);
	EXPECT_NEAR(walker.m_path[1].y(), 7.f - g_shiftF.y(), 1e-4f);
	// The runtime mesh is read again from the shifted corners.
	const auto nearest = mesh.getMesh().findNearest(math::vec3{1.f, 0.f, 1.f} - g_shiftF);
	ASSERT_TRUE(nearest.has_value());
	expectNear(nearest->position, math::vec3{1.f, 0.f, 1.f} - g_shiftF, 0.2f);
}

TEST_F(AiOriginShiftTest, SteeringTargetsFollowTheShift) {
	Scene scene;
	auto& agent = scene.createEntity("agent").addComponent<component::SteeringAgent>();
	agent.targetPoint = {8.f, 1.f, -3.f};
	agent.m_targetLast = {2.f, 0.f, 2.f};
	agent.slot = {0.f, 0.f, -2.f};
	scene.shiftWorldOrigin(g_shift);
	expectNear(agent.targetPoint, math::vec3{8.f, 1.f, -3.f} - g_shiftF);
	expectNear(agent.m_targetLast, math::vec3{2.f, 0.f, 2.f} - g_shiftF);
	// The slot is in the frame of the leader.
	expectNear(agent.slot, {0.f, 0.f, -2.f});
}

TEST_F(AiOriginShiftTest, OnlyTheKeysReadAsPointsMove) {
	TreeBuilder builder;
	builder.key("spot", math::vec3{5.f, 0.f, 5.f});
	builder.key("near", math::vec3{1.f, 1.f, 1.f});
	builder.key("heading", math::vec3{0.f, 0.f, 1.f});
	const auto root = builder.add("root");
	const auto sequence = builder.add("composite.sequence", {}, root);
	builder.add("condition.distance", {{"key", "near"}, {"op", "closer than"}, {"distance", "1"}}, sequence);
	builder.add("action.move_to", {{"key", "spot"}}, sequence);
	builder.add("action.set", {{"key", "heading"}, {"value", "0 1 0"}}, sequence);
	ai::BehaviourTreeInstance instance{mkShared<const ai::BehaviourTree>(builder.tree()), {}};
	instance.shiftOrigin(g_shiftF);
	expectNear(vectorOf(instance, "spot"), math::vec3{5.f, 0.f, 5.f} - g_shiftF);
	expectNear(vectorOf(instance, "near"), math::vec3{1.f, 1.f, 1.f} - g_shiftF);
	expectNear(vectorOf(instance, "heading"), {0.f, 0.f, 1.f});
}

TEST_F(AiOriginShiftTest, ATreeWalkerReachesItsGoalAcrossAShift) {
	TreeBuilder builder;
	builder.key("goal", math::vec3{5.f, 0.f, 0.f});
	builder.key("arrived", false);
	const auto root = builder.add("root");
	const auto sequence = builder.add("composite.sequence", {}, root);
	builder.add("action.move_to", {{"key", "goal"}, {"distance", "0.3"}}, sequence);
	builder.add("action.set", {{"key", "arrived"}, {"value", "true"}}, sequence);
	const auto path = std::filesystem::temp_directory_path() / "owl_bt_origin_shift.owlbt";
	ASSERT_TRUE(ai::saveToFile(builder.tree(), path));
	Scene scene;
	auto entity = scene.createEntity("walker");
	entity.addComponent<component::BehaviourTreeAgent>().tree = path.generic_string();
	entity.addComponent<component::SteeringAgent>();
	scene.onStartRuntime();
	core::Timestep step;
	for (int frame = 0; frame < 80; ++frame) {
		if (frame == 10)
			scene.shiftWorldOrigin(g_shift);
		step.forceUpdate(std::chrono::milliseconds(50));
		scene.onUpdateRuntime(step, false);
	}
	const auto& running = entity.getComponent<component::BehaviourTreeAgent>();
	ASSERT_TRUE(running.m_instance);
	const auto* arrived = running.m_instance->getValue("arrived");
	ASSERT_NE(arrived, nullptr);
	EXPECT_EQ(ai::toText(*arrived), "true");
	// Where the goal is in the world: 5 along x from the first origin.
	const auto& translation = entity.getComponent<component::Transform>().transform.translation();
	EXPECT_NEAR(translation.x() + g_shiftF.x(), 5.f, 0.5f);
	EXPECT_NEAR(translation.z() + g_shiftF.z(), 0.f, 0.5f);
	scene.onEndRuntime();
	std::filesystem::remove(path);
}
