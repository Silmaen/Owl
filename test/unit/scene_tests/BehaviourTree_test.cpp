/**
 * @file BehaviourTree_test.cpp
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
#include <scene/ai/BehaviourTree.h>
#include <scene/ai/BehaviourTreeInstance.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::scene;
using ai::NodeStatus;

namespace {

// Builds a tree node by node, each node placed below the previous one.
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

	void key(const std::string& iName, ai::BlackboardValue iValue) {
		m_tree.blackboard.push_back({.name = iName, .value = std::move(iValue), .description = {}});
	}

	auto tree() -> ai::BehaviourTree& { return m_tree; }

	[[nodiscard]] auto share() const -> shared<const ai::BehaviourTree> {
		return mkShared<const ai::BehaviourTree>(m_tree);
	}

private:
	ai::BehaviourTree m_tree;
	uint64_t m_next = 1;
	float m_y = 0.f;
};

class BehaviourTreeTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_scene = mkShared<Scene>();
		m_agent = m_scene->createEntity("Agent");
	}
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(50));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	shared<Scene> m_scene;
	Entity m_agent;
};

auto text(const ai::BlackboardValue* iValue) -> std::string { return iValue == nullptr ? "-" : ai::toText(*iValue); }

}// namespace

TEST_F(BehaviourTreeTest, ValuesGoToTextAndBack) {
	EXPECT_EQ(ai::toText(true), "true");
	EXPECT_EQ(ai::toText(2.5), "2.5");
	EXPECT_EQ(ai::toText(core::UUID{42}), "42");
	EXPECT_EQ(ai::toText(math::vec3{1.f, 2.f, 3.5f}), "1, 2, 3.5");
	EXPECT_EQ(ai::parseValue(ai::BlackboardType::Vector, "1, 2, 3.5"), ai::BlackboardValue{math::vec3(1.f, 2.f, 3.5f)});
	EXPECT_EQ(ai::parseValue(ai::BlackboardType::Number, " 4 "), ai::BlackboardValue{4.0});
	EXPECT_EQ(ai::parseValue(ai::BlackboardType::Entity, "7"), ai::BlackboardValue{core::UUID{7}});
	EXPECT_EQ(ai::parseValue(ai::BlackboardType::Bool, "false"), ai::BlackboardValue{false});
	EXPECT_FALSE(ai::parseValue(ai::BlackboardType::Bool, "yes").has_value());
	EXPECT_FALSE(ai::parseValue(ai::BlackboardType::Number, "fast").has_value());
	EXPECT_FALSE(ai::parseValue(ai::BlackboardType::Vector, "1, 2").has_value());
	EXPECT_EQ(ai::getDefaultValue(ai::BlackboardType::String), ai::BlackboardValue{std::string{}});
}

TEST_F(BehaviourTreeTest, ATreeSurvivesASaveAndALoad) {
	TreeBuilder builder;
	const auto root = builder.add("root");
	const auto sequence = builder.add("composite.sequence", {}, root);
	builder.add("action.wait", {{"seconds", "2"}}, sequence);
	builder.tree().nodes.back().comment = "Rest a little";
	builder.key("alert", false);
	builder.key("speed", 3.5);
	builder.key("name", std::string{"Bob"});
	builder.key("target", core::UUID{12});
	builder.key("home", math::vec3{1.f, 0.f, -2.f});
	const auto text = ai::serialize(builder.tree());
	const auto read = ai::deserialize(text, "test");
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, builder.tree());
	EXPECT_FALSE(ai::deserialize("Nothing: 1", "test").has_value());
	EXPECT_FALSE(ai::deserialize("[unclosed", "test").has_value());
	const auto path = std::filesystem::temp_directory_path() / "owl_bt_roundtrip.owlbt";
	ASSERT_TRUE(ai::saveToFile(builder.tree(), path));
	const auto loaded = ai::loadFromFile(path);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(*loaded, builder.tree());
	std::filesystem::remove(path);
}

TEST_F(BehaviourTreeTest, ChildrenRunFromTopToBottom) {
	TreeBuilder builder;
	const auto root = builder.add("root");
	const auto sequence = builder.add("composite.sequence", {}, root);
	const auto first = builder.add("action.wait", {}, sequence);
	const auto second = builder.add("action.wait", {}, sequence);
	builder.tree().findNode(first)->position[1] = 100.f;
	EXPECT_EQ(builder.tree().getChildren(sequence), (std::vector{second, first}));
	EXPECT_EQ(builder.tree().getParent(first), sequence);
	EXPECT_FALSE(builder.tree().getParent(root).has_value());
	builder.tree().removeNode(second);
	EXPECT_EQ(builder.tree().getChildren(sequence), (std::vector{first}));
}

TEST_F(BehaviourTreeTest, ValidationNamesTheProblems) {
	TreeBuilder builder;
	EXPECT_EQ(builder.tree().validate().size(), 1u);
	const auto root = builder.add("root");
	const auto inverter = builder.add("decorator.inverter", {}, root);
	builder.add("action.wait", {}, inverter);
	builder.add("action.wait", {}, inverter);
	builder.add("condition.compare", {{"key", "ghost"}});
	builder.add("action.lua");
	builder.add("decorator.unknown");
	const auto problems = builder.tree().validate();
	const auto mentions = [&problems](const std::string_view iText) -> bool {
		return std::ranges::any_of(problems,
								   [iText](const std::string& iProblem) -> bool { return iProblem.contains(iText); });
	};
	EXPECT_TRUE(mentions("Inverter has 2 children"));
	EXPECT_TRUE(mentions("'ghost'"));
	EXPECT_TRUE(mentions("names no Lua function"));
	EXPECT_TRUE(mentions("Unknown node type"));
	EXPECT_TRUE(mentions("not linked to the Root"));
}

TEST_F(BehaviourTreeTest, ASequenceStopsAtTheFirstFailure) {
	TreeBuilder builder;
	builder.key("step", 0.0);
	builder.key("done", false);
	const auto root = builder.add("root");
	const auto sequence = builder.add("composite.sequence", {}, root);
	builder.add("action.set", {{"key", "step"}, {"value", "1"}}, sequence);
	builder.add("condition.compare", {{"key", "done"}, {"op", "is set"}}, sequence);
	builder.add("action.set", {{"key", "step"}, {"value", "2"}}, sequence);
	ai::BehaviourTreeInstance instance{builder.share(), {}};
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Failure);
	EXPECT_EQ(text(instance.getValue("step")), "1");
	instance.setValue("done", true);
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Success);
	EXPECT_EQ(text(instance.getValue("step")), "2");
	EXPECT_EQ(instance.getNodeStatus(sequence), NodeStatus::Success);
	EXPECT_EQ(instance.getTickCount(), 2u);
}

TEST_F(BehaviourTreeTest, ASelectorStopsAtTheFirstSuccessAndAWaitRuns) {
	TreeBuilder builder;
	builder.key("chosen", std::string{});
	const auto root = builder.add("root");
	const auto selector = builder.add("composite.selector", {}, root);
	const auto never = builder.add("decorator.inverter", {}, selector);
	builder.add("action.set", {{"key", "chosen"}, {"value", "first"}}, never);
	const auto sequence = builder.add("composite.sequence", {}, selector);
	const auto wait = builder.add("action.wait", {{"seconds", "0.25"}}, sequence);
	builder.add("action.set", {{"key", "chosen"}, {"value", "second"}}, sequence);
	ai::BehaviourTreeInstance instance{builder.share(), {}};
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(instance.getNodeStatus(wait), NodeStatus::Running);
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Success);
	EXPECT_EQ(text(instance.getValue("chosen")), "second");
	instance.halt(m_agent);
	EXPECT_EQ(instance.getNodeStatus(wait), NodeStatus::Idle);
	EXPECT_EQ(instance.getStatus(), NodeStatus::Idle);
}

TEST_F(BehaviourTreeTest, AReactiveSelectorDropsTheRunningBranch) {
	TreeBuilder builder;
	builder.key("alert", false);
	builder.key("mode", std::string{});
	const auto root = builder.add("root");
	const auto selector = builder.add("composite.selector", {{"reactive", "true"}}, root);
	const auto chase = builder.add("composite.sequence", {}, selector);
	builder.add("condition.compare", {{"key", "alert"}, {"op", "=="}, {"value", "true"}}, chase);
	builder.add("action.set", {{"key", "mode"}, {"value", "chase"}}, chase);
	builder.add("action.wait", {{"seconds", "5"}}, chase);
	const auto patrol = builder.add("composite.sequence", {}, selector);
	builder.add("action.set", {{"key", "mode"}, {"value", "patrol"}}, patrol);
	const auto stroll = builder.add("action.wait", {{"seconds", "10"}}, patrol);
	ai::BehaviourTreeInstance instance{builder.share(), {}};
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(text(instance.getValue("mode")), "patrol");
	instance.setValue("alert", true);
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(text(instance.getValue("mode")), "chase");
	EXPECT_EQ(instance.getNodeStatus(stroll), NodeStatus::Idle);
	// Without reactivity the patrol would keep running.
	builder.tree().findNode(selector)->values["reactive"] = "false";
	ai::BehaviourTreeInstance lazy{builder.share(), {}};
	lazy.setValue("alert", false);
	std::ignore = lazy.tick(m_agent, 0.1f);
	lazy.setValue("alert", true);
	std::ignore = lazy.tick(m_agent, 0.1f);
	EXPECT_EQ(text(lazy.getValue("mode")), "patrol");
}

TEST_F(BehaviourTreeTest, AParallelFollowsItsPolicy) {
	TreeBuilder builder;
	const auto root = builder.add("root");
	const auto parallel = builder.add("composite.parallel", {{"policy", "one"}}, root);
	const auto slow = builder.add("action.wait", {{"seconds", "1"}}, parallel);
	builder.add("action.wait", {{"seconds", "0.15"}}, parallel);
	ai::BehaviourTreeInstance any{builder.share(), {}};
	EXPECT_EQ(any.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(any.tick(m_agent, 0.1f), NodeStatus::Success);
	EXPECT_EQ(any.getNodeStatus(slow), NodeStatus::Idle);
	builder.tree().findNode(parallel)->values["policy"] = "all";
	ai::BehaviourTreeInstance all{builder.share(), {}};
	for (int tick = 0; tick < 9; ++tick) EXPECT_EQ(all.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(all.tick(m_agent, 0.11f), NodeStatus::Success);
}

TEST_F(BehaviourTreeTest, DecoratorsChangeTheirChild) {
	TreeBuilder builder;
	builder.key("count", 0.0);
	builder.key("ok", false);
	const auto root = builder.add("root");
	const auto repeat = builder.add("decorator.repeat", {{"count", "3"}}, root);
	builder.add("condition.compare", {{"key", "ok"}, {"op", "is not set"}}, repeat);
	ai::BehaviourTreeInstance repeating{builder.share(), {}};
	EXPECT_EQ(repeating.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(repeating.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(repeating.tick(m_agent, 0.1f), NodeStatus::Success);

	builder.tree().findNode(repeat)->type = "decorator.retry";
	ai::BehaviourTreeInstance retrying{builder.share(), {}};
	retrying.setValue("ok", true);
	EXPECT_EQ(retrying.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(retrying.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(retrying.tick(m_agent, 0.1f), NodeStatus::Failure);

	builder.tree().findNode(repeat)->type = "decorator.cooldown";
	builder.tree().findNode(repeat)->values = {{"seconds", "0.5"}};
	ai::BehaviourTreeInstance cooling{builder.share(), {}};
	EXPECT_EQ(cooling.tick(m_agent, 0.25f), NodeStatus::Success);
	EXPECT_EQ(cooling.tick(m_agent, 0.25f), NodeStatus::Failure);
	EXPECT_EQ(cooling.tick(m_agent, 0.25f), NodeStatus::Success);

	builder.tree().findNode(repeat)->type = "decorator.succeeder";
	ai::BehaviourTreeInstance succeeding{builder.share(), {}};
	succeeding.setValue("ok", true);
	EXPECT_EQ(succeeding.tick(m_agent, 0.1f), NodeStatus::Success);
}

TEST_F(BehaviourTreeTest, ATimeoutStopsALongChild) {
	TreeBuilder builder;
	const auto root = builder.add("root");
	const auto timeout = builder.add("decorator.timeout", {{"seconds", "0.25"}}, root);
	const auto wait = builder.add("action.wait", {{"seconds", "10"}}, timeout);
	ai::BehaviourTreeInstance instance{builder.share(), {}};
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Running);
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Failure);
	EXPECT_EQ(instance.getNodeStatus(wait), NodeStatus::Idle);
}

TEST_F(BehaviourTreeTest, AGuardStopsItsChildWhenItNoLongerHolds) {
	TreeBuilder builder;
	builder.key("health", 10.0);
	const auto root = builder.add("root");
	const auto guard = builder.add("decorator.guard", {{"key", "health"}, {"op", ">"}, {"value", "3"}}, root);
	const auto wait = builder.add("action.wait", {{"seconds", "10"}}, guard);
	ai::BehaviourTreeInstance instance{builder.share(), {}};
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Running);
	instance.setValue("health", 2.0);
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Failure);
	EXPECT_EQ(instance.getNodeStatus(wait), NodeStatus::Idle);
}

TEST_F(BehaviourTreeTest, DistanceAndPlayerReadTheScene) {
	auto hero = m_scene->createEntity("Hero");
	hero.addComponent<component::Player>().primary = true;
	hero.getComponent<component::Transform>().transform.translation() = {3.f, 0.f, 0.f};
	TreeBuilder builder;
	builder.key("player", core::UUID{0});
	const auto root = builder.add("root");
	const auto sequence = builder.add("composite.sequence", {}, root);
	builder.add("action.find_player", {{"key", "player"}}, sequence);
	builder.add("condition.distance", {{"key", "player"}, {"op", "closer than"}, {"distance", "5"}}, sequence);
	ai::BehaviourTreeInstance instance{builder.share(), {}};
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Success);
	EXPECT_EQ(text(instance.getValue("player")), ai::toText(hero.getUUID()));
	hero.getComponent<component::Transform>().transform.translation() = {30.f, 0.f, 0.f};
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Failure);
	// Measured on the ground, the height of the player does not count.
	hero.getComponent<component::Transform>().transform.translation() = {1.f, 8.f, 0.f};
	EXPECT_EQ(instance.tick(m_agent, 0.1f), NodeStatus::Failure);
	builder.tree().nodes.back().values["space"] = "ground";
	ai::BehaviourTreeInstance grounded{builder.share(), {}};
	EXPECT_EQ(grounded.tick(m_agent, 0.1f), NodeStatus::Success);
}

TEST_F(BehaviourTreeTest, OverridesReplaceTheValuesOfTheTree) {
	TreeBuilder builder;
	builder.key("speed", 1.0);
	builder.key("name", std::string{"guard"});
	builder.add("root");
	const std::vector<ai::BlackboardKey> overrides{{.name = "speed", .value = 4.0, .description = {}},
												   {.name = "name", .value = 2.0, .description = {}},
												   {.name = "ghost", .value = true, .description = {}}};
	const ai::BehaviourTreeInstance instance{builder.share(), overrides};
	EXPECT_EQ(text(instance.getValue("speed")), "4");
	EXPECT_EQ(text(instance.getValue("name")), "guard");
	EXPECT_EQ(instance.getValue("ghost"), nullptr);
}

TEST_F(BehaviourTreeTest, TheSameRandomTreeMakesTheSameChoices) {
	TreeBuilder builder;
	builder.key("picked", std::string{});
	const auto root = builder.add("root");
	const auto random = builder.add("composite.random", {}, root);
	for (const char* name: {"a", "b", "c", "d"})
		builder.add("action.set", {{"key", "picked"}, {"value", name}}, random);
	const auto picks = [&builder]() -> std::string {
		auto scene = mkShared<Scene>();
		scene->setRandomSeed(7);
		const auto agent = scene->createEntity("Agent");
		ai::BehaviourTreeInstance instance{builder.share(), {}};
		std::string sequence;
		for (int tick = 0; tick < 12; ++tick) {
			std::ignore = instance.tick(agent, 0.1f);
			sequence += text(instance.getValue("picked"));
		}
		return sequence;
	};
	const auto first = picks();
	EXPECT_EQ(first, picks());
	EXPECT_NE(first, std::string(12, 'a'));
}

TEST_F(BehaviourTreeTest, AnAgentWalksToItsTargetWithItsSteering) {
	TreeBuilder builder;
	builder.key("goal", math::vec3{5.f, 0.f, 0.f});
	builder.key("arrived", false);
	const auto root = builder.add("root");
	const auto sequence = builder.add("composite.sequence", {}, root);
	builder.add("action.move_to", {{"key", "goal"}, {"distance", "0.3"}}, sequence);
	builder.add("action.set", {{"key", "arrived"}, {"value", "true"}}, sequence);
	const auto path = std::filesystem::temp_directory_path() / "owl_bt_walker.owlbt";
	ASSERT_TRUE(ai::saveToFile(builder.tree(), path));
	auto& agent = m_agent.addComponent<component::BehaviourTreeAgent>();
	agent.tree = path.generic_string();
	m_agent.addComponent<component::SteeringAgent>();
	m_scene->onStartRuntime();
	run(*m_scene, 60);
	const auto& running = m_agent.getComponent<component::BehaviourTreeAgent>();
	ASSERT_TRUE(running.m_instance);
	EXPECT_EQ(text(running.m_instance->getValue("arrived")), "true");
	EXPECT_NEAR(m_agent.getComponent<component::Transform>().transform.translation().x(), 5.f, 0.5f);
	// Switched off, the tree stops.
	m_agent.getComponent<component::BehaviourTreeAgent>().enabled = false;
	run(*m_scene, 1);
	EXPECT_EQ(running.m_instance->getStatus(), NodeStatus::Idle);
	m_scene->onEndRuntime();
	std::filesystem::remove(path);
}

TEST_F(BehaviourTreeTest, TheComponentSurvivesASaveAndALoad) {
	auto& agent = m_agent.addComponent<component::BehaviourTreeAgent>();
	agent.enabled = false;
	agent.tree = "ai/guard.owlbt";
	agent.overrides = {{.name = "speed", .value = 4.5, .description = {}},
					   {.name = "home", .value = math::vec3{1.f, 2.f, 3.f}, .description = {}},
					   {.name = "target", .value = core::UUID{99}, .description = {}},
					   {.name = "angry", .value = true, .description = {}},
					   {.name = "name", .value = std::string{"Rex"}, .description = {}}};
	const auto saved = SceneSerializer(m_scene).serializeToString();
	auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer({saved.begin(), saved.end()}));
	const auto& read = loaded->findEntityByUUID(m_agent.getUUID()).getComponent<component::BehaviourTreeAgent>();
	EXPECT_FALSE(read.enabled);
	EXPECT_EQ(read.tree, "ai/guard.owlbt");
	EXPECT_EQ(read.overrides, agent.overrides);
}
