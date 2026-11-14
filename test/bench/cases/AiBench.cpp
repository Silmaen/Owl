/**
 * @file AiBench.cpp
 * @author Silmaen
 * @date 13/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/ai/BehaviourTree.h>
#include <scene/ai/BehaviourTreeInstance.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace owl::bench {

namespace {

constexpr uint32_t g_Agents = 1000;
constexpr uint32_t g_Columns = 40;
constexpr float g_Spacing = 1.5f;
constexpr uint32_t g_SteerFrames = 10;
constexpr uint32_t g_SteerWarmFrames = 10;

// Builds a tree node by node, each node below the previous one (children run from top to bottom).
class TreeBuilder {
public:
	auto add(const std::string& iType, std::map<std::string, std::string> iValues = {},
			 const std::optional<core::UUID> iParent = std::nullopt) -> core::UUID {
		const core::UUID id{m_next++};
		m_tree.nodes.push_back(
				{.id = id, .type = iType, .position = {0.f, m_y}, .values = std::move(iValues), .comment = {}});
		m_y += 10.f;
		if (iParent.has_value())
			m_tree.links.push_back({.parent = *iParent, .child = id});
		return id;
	}

	void key(const std::string& iName, scene::ai::BlackboardValue iValue) {
		m_tree.blackboard.push_back({.name = iName, .value = std::move(iValue), .description = {}});
	}

	[[nodiscard]] auto share() const -> shared<const scene::ai::BehaviourTree> {
		return mkShared<const scene::ai::BehaviourTree>(m_tree);
	}

private:
	scene::ai::BehaviourTree m_tree;
	uint64_t m_next = 1;
	float m_y = 0.f;
};

// A guard: chases its target when closer than 5, else wanders a while and counts its rounds (reactive selector).
auto makeGuardTree() -> shared<const scene::ai::BehaviourTree> {
	TreeBuilder builder;
	builder.key("target", math::vec3{0.f, 0.f, 0.f});
	builder.key("rounds", 0.0);
	const auto root = builder.add("root");
	const auto selector = builder.add("composite.selector", {{"reactive", "true"}}, root);
	const auto chase = builder.add("composite.sequence", {}, selector);
	builder.add("condition.distance", {{"key", "target"}, {"op", "closer than"}, {"distance", "5"}, {"space", "plane"}},
				chase);
	builder.add("action.steer", {{"behaviour", "seek"}, {"key", "target"}}, chase);
	const auto patrol = builder.add("composite.sequence", {}, selector);
	builder.add("action.steer", {{"behaviour", "wander"}}, patrol);
	builder.add("action.wait", {{"seconds", "0.5"}, {"spread", "0.5"}}, patrol);
	builder.add("action.set", {{"key", "rounds"}, {"value", "1"}}, patrol);
	return builder.share();
}

auto gridPosition(const uint32_t iIndex) -> math::vec3 {
	return {g_Spacing * static_cast<float>(iIndex % g_Columns), g_Spacing * static_cast<float>(iIndex / g_Columns),
			0.f};
}

// Agents of a 2D flock on a grid, each one with a few neighbours within its radius.
auto makeFlock(const uint32_t iCount) -> shared<scene::Scene> {
	auto scn = mkShared<scene::Scene>();
	scn->setRandomSeed(2026);
	for (uint32_t i = 0; i < iCount; ++i) {
		auto entity = scn->createEntity("bird");
		entity.getComponent<scene::component::Transform>().transform.translation() = gridPosition(i);
		auto& agent = entity.addComponent<scene::component::SteeringAgent>();
		agent.plane = scene::ai::SteeringPlane::Plane2D;
		agent.flock = "birds";
		agent.maxSpeed = 3.f;
		agent.radius = 0.4f;
		agent.weights = {.wander = 0.5f, .separation = 2.f, .alignment = 1.f, .cohesion = 1.f, .avoidance = 1.f};
	}
	return scn;
}

auto makeFrame() -> core::Timestep {
	core::Timestep frame;
	frame.forceUpdate(std::chrono::microseconds(16'667));
	return frame;
}

}// namespace

void runAiBenches(Runner& ioRunner) {
	if (!ioRunner.wants("ai"))
		return;
	if (ioRunner.wants("ai/behaviour_tree")) {
		const auto tree = makeGuardTree();
		auto scn = mkShared<scene::Scene>();
		scn->setRandomSeed(2026);
		std::vector<std::pair<scene::Entity, uniq<scene::ai::BehaviourTreeInstance>>> agents;
		agents.reserve(g_Agents);
		for (uint32_t i = 0; i < g_Agents; ++i) {
			auto entity = scn->createEntity("guard");
			entity.getComponent<scene::component::Transform>().transform.translation() = gridPosition(i);
			entity.addComponent<scene::component::SteeringAgent>();
			// One guard in two has its target in reach: both branches of the selector run.
			const math::vec3 target = gridPosition(i) + math::vec3{i % 2 == 0 ? 3.f : 30.f, 0.f, 0.f};
			const std::vector<scene::ai::BlackboardKey> overrides{
					{.name = "target", .value = target, .description = {}}};
			agents.emplace_back(entity, mkUniq<scene::ai::BehaviourTreeInstance>(tree, overrides));
		}
		ioRunner.measure(std::format("ai/behaviour_tree/tick/{}_agents", g_Agents), g_Agents, [&]() -> void {
			for (auto& [entity, instance]: agents) doNotOptimize(instance->tick(entity, 1.f / 60.f));
		});
	}
	if (ioRunner.wants("ai/steering")) {
		const auto frame = makeFrame();
		shared<scene::Scene> scn;
		const auto release = [&scn]() -> void {
			if (scn)
				scn->onEndRuntime();
			scn.reset();
		};
		ioRunner.measureWithSetup(
				std::format("ai/steering/flock/{}_agents", g_Agents), g_SteerFrames,
				[&]() -> void {
					release();
					scn = makeFlock(g_Agents);
					scn->onStartRuntime();
					// The flock gets under way: the agents move and see their neighbours move.
					for (uint32_t f = 0; f < g_SteerWarmFrames; ++f) scn->onUpdateRuntime(frame, false);
				},
				[&]() -> void {
					for (uint32_t f = 0; f < g_SteerFrames; ++f) scn->onUpdateRuntime(frame, false);
				});
		release();
	}
}

}// namespace owl::bench
