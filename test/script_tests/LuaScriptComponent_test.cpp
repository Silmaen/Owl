/**
 * @file LuaScriptComponent_test.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/LuaScript.h>
#include <script/ScriptEngine.h>

#include <fstream>

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
	const auto dir = owl::test::getRootPath() / "output" / "test_tmp";
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
		serializer.serialize(scenePath);
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
	const auto dir = owl::test::getRootPath() / "output" / "test_tmp";
	std::filesystem::create_directories(dir);
	const auto scenePath = dir / "lua_empty_props_test.owl";

	{
		auto scn = mkShared<Scene>();
		auto entity = scn->createEntity("EmptyProps");
		entity.addComponent<component::LuaScript>().scriptPath = "scripts/empty.lua";
		const SceneSerializer serializer(scn);
		serializer.serialize(scenePath);
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
	const auto dir = owl::test::getRootPath() / "output" / "test_tmp";
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
	const auto dir = owl::test::getRootPath() / "output" / "test_tmp";
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
