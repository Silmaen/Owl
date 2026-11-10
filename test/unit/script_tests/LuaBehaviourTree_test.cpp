/**
 * @file LuaBehaviourTree_test.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/GameState.h>
#include <scene/Scene.h>
#include <scene/ai/BehaviourTree.h>
#include <scene/ai/BehaviourTreeInstance.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <utility>

using namespace owl;
using namespace owl::scene;

namespace {

/// Leaves of the tree and a script driving its blackboard and the steering.
constexpr auto g_Script = R"(
local ticks = 0

function is_hungry(entity, dt)
	return bt.get_value(entity, "hunger") > 2
end

function eat(entity, dt)
	ticks = ticks + 1
	if ticks < 3 then
		return "running"
	end
	gamestate.set("reset", bt.set_value(entity, "hunger", 0))
	gamestate.set("after", bt.get_value(entity, "hunger"))
	gamestate.set("meals", (gamestate.get("meals") or 0) + 1)
	return true
end

function on_create()
	bt.set_value(entity_id, "hunger", 5)
	steer.set_weight(entity_id, "wander", 0.5)
	steer.set_target_point(entity_id, 1, 2, 3)
end

function on_update(dt)
	gamestate.set("status", bt.get_status(entity_id))
	gamestate.set("wrong_kind", bt.set_value(entity_id, "hunger", "lots"))
	gamestate.set("wander", steer.get_weight(entity_id, "wander"))
	gamestate.set("unknown", steer.set_weight(entity_id, "dance", 1))
end
)";

class LuaBehaviourTree : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_lua_bt_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir);
		std::ofstream{m_dir / "cow.lua"} << g_Script;
		ai::BehaviourTree tree;
		tree.blackboard.push_back({.name = "hunger", .value = 0.0, .description = {}});
		const auto node = [&tree](const uint64_t iId, const std::string& iType, const float iX,
								  std::map<std::string, std::string> iValues) -> void {
			tree.nodes.push_back({.id = core::UUID{iId},
								  .type = iType,
								  .position = {0.f, iX},
								  .values = std::move(iValues),
								  .comment = {}});
		};
		node(1, "root", 0.f, {});
		node(2, "composite.sequence", 0.f, {});
		node(3, "condition.lua", 0.f, {{"function", "is_hungry"}});
		node(4, "action.lua", 10.f, {{"function", "eat"}});
		tree.links = {{.parent = core::UUID{1}, .child = core::UUID{2}},
					  {.parent = core::UUID{2}, .child = core::UUID{3}},
					  {.parent = core::UUID{2}, .child = core::UUID{4}}};
		ASSERT_TRUE(ai::saveToFile(tree, m_dir / "cow.owlbt"));
	}

	void TearDown() override {
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(LuaBehaviourTree, LuaLeavesRunAndTheScriptReadsTheBlackboard) {
	const auto scene = mkShared<Scene>();
	auto cow = scene->createEntityWithUUID(core::UUID{1}, "Cow");
	cow.addComponent<component::LuaScript>().scriptPath = (m_dir / "cow.lua").generic_string();
	cow.addComponent<component::BehaviourTreeAgent>().tree = (m_dir / "cow.owlbt").generic_string();
	cow.addComponent<component::SteeringAgent>().weights.avoidance = 0.f;
	scene->onStartRuntime();
	core::Timestep step;
	for (int frame = 0; frame < 20; ++frame) {
		step.forceUpdate(std::chrono::milliseconds(50));
		scene->onUpdateRuntime(step, false);
	}
	const auto& state = scene->getGameState();
	EXPECT_EQ(state.get("reset"), (GameState::Value{true}));
	EXPECT_EQ(state.get("after"), (GameState::Value{int64_t{0}}));
	EXPECT_EQ(state.get("meals"), (GameState::Value{int64_t{1}}));
	EXPECT_EQ(state.get("wrong_kind"), (GameState::Value{false}));
	EXPECT_EQ(state.get("unknown"), (GameState::Value{false}));
	EXPECT_EQ(state.get("wander"), (GameState::Value{0.5f}));
	EXPECT_EQ(state.get("status"), (GameState::Value{std::string{"failure"}}));
	const auto& agent = cow.getComponent<component::BehaviourTreeAgent>();
	ASSERT_TRUE(agent.m_instance);
	EXPECT_EQ(ai::toText(*agent.m_instance->getValue("hunger")), "0");
	EXPECT_EQ(cow.getComponent<component::SteeringAgent>().targetPoint, math::vec3(1.f, 2.f, 3.f));
	scene->onEndRuntime();
}
