/**
 * @file LuaScriptComponent_test.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <input/Input.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/LuaScript.h>
#include <script/ScriptEngine.h>
#include <window/Window.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>

using namespace owl;
using namespace owl::scene;
using namespace owl::script;

namespace {
auto writeTempScript(const std::filesystem::path& iDir, const std::string& iFilename, const std::string& iContent)
		-> std::filesystem::path {
	std::filesystem::create_directories(iDir);
	const auto path = iDir / iFilename;
	std::ofstream file(path);
	file << iContent;
	file.close();
	return path;
}

}// namespace

TEST(LuaScriptComponent, createAndAccess) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("ScriptedEntity");
	auto& comp = entity.addComponent<component::LuaScript>();

	comp.scriptPath = "scripts/player.lua";
	comp.properties.push_back({.name = "speed", .type = ScriptPropertyType::Float, .value = 5.0f});
	comp.properties.push_back({.name = "name", .type = ScriptPropertyType::String, .value = std::string("hero")});

	EXPECT_EQ(comp.scriptPath, "scripts/player.lua");
	EXPECT_EQ(comp.properties.size(), 2);
	EXPECT_EQ(comp.properties[0].name, "speed");
	EXPECT_NEAR(std::get<float>(comp.properties[0].value), 5.0f, 0.01f);
	EXPECT_EQ(comp.properties[1].name, "name");
	EXPECT_EQ(std::get<std::string>(comp.properties[1].value), "hero");
	EXPECT_EQ(comp.instance, nullptr);

	core::Log::invalidate();
}

TEST(LuaScriptComponent, serializeDeserializeViaScene) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_luascriptcomponent_test_1";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto scenePath = dir / "lua_serialize_test.owl";

	// Build a scene with a LuaScript component.
	{
		auto scn = mkShared<Scene>();
		auto entity = scn->createEntity("ScriptEntity");
		auto& comp = entity.addComponent<component::LuaScript>();
		comp.scriptPath = "scripts/test.lua";
		comp.properties.push_back({.name = "speed", .type = ScriptPropertyType::Float, .value = 3.14f});
		comp.properties.push_back({.name = "count", .type = ScriptPropertyType::Int, .value = int64_t{42}});
		comp.properties.push_back({.name = "label", .type = ScriptPropertyType::String, .value = std::string("abc")});
		comp.properties.push_back({.name = "active", .type = ScriptPropertyType::Bool, .value = true});

		const SceneSerializer serializer(scn);
		EXPECT_TRUE(serializer.serialize(scenePath));
	}

	// Reload from YAML.
	{
		auto scn = mkShared<Scene>();
		const SceneSerializer serializer(scn);
		ASSERT_TRUE(serializer.deserialize(scenePath));

		const auto entities = scn->getAllEntities();
		ASSERT_EQ(entities.size(), 1);
		ASSERT_TRUE(entities[0].hasComponent<component::LuaScript>());

		const auto& comp = entities[0].getComponent<component::LuaScript>();
		EXPECT_EQ(comp.scriptPath, "scripts/test.lua");
		ASSERT_EQ(comp.properties.size(), 4);

		EXPECT_EQ(comp.properties[0].name, "speed");
		EXPECT_EQ(comp.properties[0].type, ScriptPropertyType::Float);
		EXPECT_NEAR(std::get<float>(comp.properties[0].value), 3.14f, 0.01f);

		EXPECT_EQ(comp.properties[1].name, "count");
		EXPECT_EQ(comp.properties[1].type, ScriptPropertyType::Int);
		EXPECT_EQ(std::get<int64_t>(comp.properties[1].value), 42);

		EXPECT_EQ(comp.properties[2].name, "label");
		EXPECT_EQ(comp.properties[2].type, ScriptPropertyType::String);
		EXPECT_EQ(std::get<std::string>(comp.properties[2].value), "abc");

		EXPECT_EQ(comp.properties[3].name, "active");
		EXPECT_EQ(comp.properties[3].type, ScriptPropertyType::Bool);
		EXPECT_TRUE(std::get<bool>(comp.properties[3].value));

		EXPECT_EQ(comp.instance, nullptr);
	}

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(LuaScriptComponent, serializeEmptyProperties) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_luascriptcomponent_test_2";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto scenePath = dir / "lua_empty_props_test.owl";

	{
		auto scn = mkShared<Scene>();
		auto entity = scn->createEntity("EmptyProps");
		entity.addComponent<component::LuaScript>().scriptPath = "scripts/empty.lua";
		const SceneSerializer serializer(scn);
		EXPECT_TRUE(serializer.serialize(scenePath));
	}
	{
		auto scn = mkShared<Scene>();
		const SceneSerializer serializer(scn);
		ASSERT_TRUE(serializer.deserialize(scenePath));
		const auto entities = scn->getAllEntities();
		ASSERT_EQ(entities.size(), 1);
		const auto& comp = entities[0].getComponent<component::LuaScript>();
		EXPECT_EQ(comp.scriptPath, "scripts/empty.lua");
		EXPECT_TRUE(comp.properties.empty());
	}

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(LuaScriptComponent, emptyScriptPathSkipped) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("EmptyScript");
	entity.addComponent<component::LuaScript>().scriptPath = "";

	// onStartRuntime should not crash with empty scriptPath.
	scn->onStartRuntime();
	auto& comp = entity.getComponent<component::LuaScript>();
	EXPECT_EQ(comp.instance, nullptr);
	scn->onEndRuntime();

	core::Log::invalidate();
}

TEST(LuaScriptComponent, sceneLifecycle) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_luascriptcomponent_test_3";
	std::filesystem::remove_all(dir);
	const auto path = writeTempScript(dir, "lifecycle_comp.lua",
									  "created = false\n"
									  "updated_count = 0\n"
									  "destroyed = false\n"
									  "function on_create()\n"
									  "  created = true\n"
									  "end\n"
									  "function on_update(dt)\n"
									  "  updated_count = updated_count + 1\n"
									  "end\n"
									  "function on_destroy()\n"
									  "  destroyed = true\n"
									  "end\n");

	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("ScriptEntity");
	entity.addComponent<component::LuaScript>().scriptPath = path.string();

	// Start runtime — creates instance and calls on_create.
	scn->onStartRuntime();
	auto& comp = entity.getComponent<component::LuaScript>();
	ASSERT_NE(comp.instance, nullptr);
	ASSERT_TRUE(comp.instance->isValid());
	EXPECT_TRUE(comp.instance->getPropertyBool("created").value_or(false));

	// Update — calls on_update.
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(16));
	scn->onUpdateRuntime(ts);
	const auto count = comp.instance->getPropertyInt("updated_count");
	ASSERT_TRUE(count.has_value());
	EXPECT_GE(count.value(), 1);

	// End runtime — calls on_destroy and resets instance.
	scn->onEndRuntime();
	auto& compAfter = entity.getComponent<component::LuaScript>();
	EXPECT_EQ(compAfter.instance, nullptr);

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(LuaScriptComponent, propertiesAppliedOnStart) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_luascriptcomponent_test_4";
	std::filesystem::remove_all(dir);
	const auto path = writeTempScript(dir, "props_apply.lua",
									  "speed = 0\n"
									  "label = ''\n"
									  "function on_create()\n"
									  "end\n");

	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("PropEntity");
	{
		auto& comp = entity.addComponent<component::LuaScript>();
		comp.scriptPath = path.string();
		comp.properties.push_back({.name = "speed", .type = ScriptPropertyType::Float, .value = 7.5f});
		comp.properties.push_back(
				{.name = "label", .type = ScriptPropertyType::String, .value = std::string("player1")});
	}

	scn->onStartRuntime();
	auto& comp = entity.getComponent<component::LuaScript>();
	ASSERT_NE(comp.instance, nullptr);

	EXPECT_NEAR(comp.instance->getPropertyFloat("speed").value_or(0), 7.5f, 0.01f);
	EXPECT_EQ(comp.instance->getPropertyString("label").value_or(""), "player1");

	scn->onEndRuntime();
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(LuaScriptComponent, onTickRunsAtTheFixedRate) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_luascriptcomponent_test_tick";
	std::filesystem::remove_all(dir);
	const auto path = writeTempScript(dir, "tick_comp.lua",
									  "ticks = 0\n"
									  "updates = 0\n"
									  "tick_dt = 0\n"
									  "tick_delta = 0\n"
									  "alpha = -1\n"
									  "function on_update(dt)\n"
									  "  updates = updates + 1\n"
									  "  alpha = time.tick_alpha()\n"
									  "end\n"
									  "function on_tick(dt)\n"
									  "  ticks = ticks + 1\n"
									  "  tick_dt = dt\n"
									  "  tick_delta = time.delta()\n"
									  "end\n");
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("TickEntity");
	entity.addComponent<component::LuaScript>().scriptPath = path.string();
	scn->getSimulationClock().setSettings({.tickRate = 50.f});
	scn->onStartRuntime();
	const auto& comp = entity.getComponent<component::LuaScript>();
	ASSERT_TRUE(comp.instance && comp.instance->isValid());

	// 50 Hz: 20 ms per tick. 10 ms frames tick every other frame; one 70 ms frame runs 3 ticks and keeps 10 ms.
	core::Timestep ts;
	for (int i = 0; i < 4; ++i) {
		ts.forceUpdate(std::chrono::milliseconds(10));
		scn->onUpdateRuntime(ts, false);
	}
	EXPECT_EQ(comp.instance->getPropertyInt("updates").value_or(0), 4);
	EXPECT_EQ(comp.instance->getPropertyInt("ticks").value_or(0), 2);
	ts.forceUpdate(std::chrono::milliseconds(70));
	scn->onUpdateRuntime(ts, false);
	EXPECT_EQ(comp.instance->getPropertyInt("ticks").value_or(0), 5);
	EXPECT_NEAR(comp.instance->getPropertyFloat("tick_dt").value_or(0.f), 0.02f, 1e-5f);
	EXPECT_NEAR(comp.instance->getPropertyFloat("tick_delta").value_or(0.f), 0.02f, 1e-5f);
	EXPECT_NEAR(comp.instance->getPropertyFloat("alpha").value_or(-1.f), 0.5f, 1e-3f);
	EXPECT_EQ(scn->getSimulationClock().getTotalTickCount(), 5u);
	scn->onEndRuntime();

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(LuaScriptComponent, inputEdgesFollowTheTicksAndTheFrames) {
	core::Log::init(core::Log::Level::Off);
	input::Input::init(window::Type::Null);
	const auto dir = std::filesystem::temp_directory_path() / "owl_luascriptcomponent_test_edges";
	std::filesystem::remove_all(dir);
	const auto path = writeTempScript(dir, "edges.lua",
									  "tick_presses = 0\n"
									  "tick_releases = 0\n"
									  "frame_presses = 0\n"
									  "clicks = 0\n"
									  "wheel = 0\n"
									  "function on_update(dt)\n"
									  "  if input.is_key_just_pressed(32) then frame_presses = frame_presses + 1 end\n"
									  "end\n"
									  "function on_tick(dt)\n"
									  "  if input.is_key_just_pressed(32) then tick_presses = tick_presses + 1 end\n"
									  "  if input.is_key_just_released(32) then tick_releases = tick_releases + 1 end\n"
									  "  if input.is_mouse_button_just_pressed(0) then clicks = clicks + 1 end\n"
									  "  if input.is_mouse_button_just_released(0) then clicks = clicks + 10 end\n"
									  "  local _, dy = input.get_mouse_wheel()\n"
									  "  wheel = wheel + dy\n"
									  "end\n");
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("EdgeEntity");
	entity.addComponent<component::LuaScript>().scriptPath = path.string();
	scn->getSimulationClock().setSettings({.tickRate = 50.f});
	scn->onStartRuntime();
	const auto& comp = entity.getComponent<component::LuaScript>();
	ASSERT_TRUE(comp.instance && comp.instance->isValid());

	core::Timestep ts;
	input::Input::injectKey(input::key::Space);
	input::Input::injectMouseButton(input::mouse::ButtonLeft);
	input::Input::addWheel({0.f, 1.f});
	input::Input::beginFrame();
	ts.forceUpdate(std::chrono::milliseconds(70));// three ticks in one frame
	scn->onUpdateRuntime(ts, false);
	input::Input::beginFrame();
	input::Input::injectKey(input::key::Space);
	input::Input::injectMouseButton(input::mouse::ButtonLeft);
	ts.forceUpdate(std::chrono::milliseconds(20));
	scn->onUpdateRuntime(ts, false);
	EXPECT_EQ(comp.instance->getPropertyInt("tick_presses").value_or(0), 1);
	EXPECT_EQ(comp.instance->getPropertyInt("tick_releases").value_or(0), 1);
	EXPECT_EQ(comp.instance->getPropertyInt("frame_presses").value_or(0), 1);
	EXPECT_EQ(comp.instance->getPropertyInt("clicks").value_or(0), 11);
	EXPECT_NEAR(comp.instance->getPropertyFloat("wheel").value_or(0.f), 1.f, 1e-5f);
	scn->onEndRuntime();

	input::Input::invalidate();
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
