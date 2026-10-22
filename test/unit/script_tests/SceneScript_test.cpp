/**
 * @file SceneScript_test.cpp
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
#include <scene/SceneSerializer.h>
#include <scene/component/Camera.h>
#include <scene/component/LuaScript.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class SceneScriptTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_scene_script_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir / "scripts");
		std::filesystem::create_directories(m_dir / "scenes");
	}

	void TearDown() override {
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	void write(const std::string& iRelative, const std::string& iText) const {
		std::ofstream{m_dir / iRelative} << iText;
	}

	[[nodiscard]] auto loadSceneFile(const std::string& iRelative) const -> shared<Scene> {
		auto scene = mkShared<Scene>();
		if (const auto result = SceneSerializer(scene).deserialize(m_dir / iRelative); !result) {
			ADD_FAILURE() << describe(result.error());
			return nullptr;
		}
		return scene;
	}

	std::filesystem::path m_dir;
};

// A format-6 scene: a primary camera carrying a script with one property.
auto cameraScene(const std::string& iScript) -> std::string {
	auto scene = mkShared<Scene>();
	auto camera = scene->createEntity("Camera");
	camera.addComponent<component::Camera>().primary = true;
	auto& luaScript = camera.addComponent<component::LuaScript>();
	luaScript.scriptPath = "scripts/" + iScript + ".lua";
	luaScript.properties.push_back({.name = "speed", .type = script::ScriptPropertyType::Float, .value = 2.5f});
	auto text = SceneSerializer(scene).serializeToString();
	const auto version = text.find("FormatVersion: 7");
	text.replace(version, 16, "FormatVersion: 6");
	return text;
}

}// namespace

TEST_F(SceneScriptTest, TheSceneScriptRoundTripsThroughTheFile) {
	auto scene = mkShared<Scene>();
	scene->getSceneScript().scriptPath = "scripts/director.lua";
	scene->getSceneScript().properties.push_back({.name = "limit", .type = script::ScriptPropertyType::Int, .value = int64_t{3}});
	ASSERT_TRUE(SceneSerializer(scene).serialize(m_dir / "scenes/level.owl"));
	const auto loaded = loadSceneFile("scenes/level.owl");
	ASSERT_NE(loaded, nullptr);
	EXPECT_EQ(loaded->getSceneScript().scriptPath, "scripts/director.lua");
	ASSERT_EQ(loaded->getSceneScript().properties.size(), 1u);
	EXPECT_EQ(std::get<int64_t>(loaded->getSceneScript().properties[0].value), 3);
	EXPECT_EQ(Scene::copy(loaded)->getSceneScript().scriptPath, "scripts/director.lua");
}

TEST_F(SceneScriptTest, ASceneWithoutScriptWritesNoKey) {
	const auto text = SceneSerializer(mkShared<Scene>()).serializeToString();
	EXPECT_EQ(text.find("SceneScript"), std::string::npos);
}

TEST_F(SceneScriptTest, MigrationMovesACameraScriptThatOnlyDrivesTheScene) {
	write("scripts/director.lua", "function on_update(dt)\n  gamestate.set('seen', 1)\nend\n");
	write("scenes/level.owl", cameraScene("director"));
	const auto scene = loadSceneFile("scenes/level.owl");
	ASSERT_NE(scene, nullptr);
	EXPECT_EQ(scene->getSceneScript().scriptPath, "scripts/director.lua");
	ASSERT_EQ(scene->getSceneScript().properties.size(), 1u);
	EXPECT_EQ(scene->getSceneScript().properties[0].name, "speed");
	for (const auto& entity: scene->getAllEntities())
		EXPECT_FALSE(entity.hasComponent<component::LuaScript>());
}

TEST_F(SceneScriptTest, MigrationKeepsACameraScriptThatMovesItsEntity) {
	write("scripts/follow.lua", "function on_update(dt)\n  transform.get_position(entity_id)\nend\n");
	write("scenes/level.owl", cameraScene("follow"));
	const auto scene = loadSceneFile("scenes/level.owl");
	ASSERT_NE(scene, nullptr);
	EXPECT_TRUE(scene->getSceneScript().scriptPath.empty());
	size_t scripted = 0;
	for (const auto& entity: scene->getAllEntities())
		scripted += entity.hasComponent<component::LuaScript>() ? 1 : 0;
	EXPECT_EQ(scripted, 1u);
}

TEST_F(SceneScriptTest, MigrationKeepsAScriptItCannotFind) {
	write("scenes/level.owl", cameraScene("missing"));
	const auto scene = loadSceneFile("scenes/level.owl");
	ASSERT_NE(scene, nullptr);
	EXPECT_TRUE(scene->getSceneScript().scriptPath.empty());
}

TEST_F(SceneScriptTest, TheSceneScriptRunsBeforeTheEntityScriptsWithoutEntityId) {
	write("scripts/director.lua",
		  "function on_create()\n  gamestate.set('has_self', entity_id ~= nil and 1 or 0)\nend\n"
		  "function on_update(dt)\n  gamestate.set('order', 1)\nend\n");
	write("scripts/actor.lua",
		  "function on_update(dt)\n  gamestate.set('seen', gamestate.get('order') or 0)\nend\n");
	auto scene = mkShared<Scene>();
	scene->createEntity("Actor").addComponent<component::LuaScript>().scriptPath = (m_dir / "scripts/actor.lua").string();
	scene->getSceneScript().scriptPath = (m_dir / "scripts/director.lua").string();
	scene->onStartRuntime();
	ASSERT_TRUE(scene->getSceneScript().instance && scene->getSceneScript().instance->isValid());
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(16));
	scene->onUpdateRuntime(step, false);
	const auto readInt = [&scene](const char* iKey) -> int64_t {
		const auto value = scene->getGameState().get(iKey);
		return value ? std::get<int64_t>(*value) : -1;
	};
	EXPECT_EQ(readInt("has_self"), 0);
	EXPECT_EQ(readInt("seen"), 1);
	scene->onEndRuntime();
	EXPECT_EQ(scene->getSceneScript().instance, nullptr);
}

TEST(LuaScriptYaml, ToYamlAndFromYamlRoundTrip) {
	component::LuaScript script;
	script.scriptPath = "scripts/a.lua";
	script.properties.push_back({.name = "label", .type = script::ScriptPropertyType::String, .value = std::string{"x"}});
	script.properties.push_back({.name = "on", .type = script::ScriptPropertyType::Bool, .value = true});
	const auto back = component::LuaScript::fromYaml(script.toYaml());
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->scriptPath, "scripts/a.lua");
	ASSERT_EQ(back->properties.size(), 2u);
	EXPECT_EQ(std::get<std::string>(back->properties[0].value), "x");
	EXPECT_TRUE(std::get<bool>(back->properties[1].value));
	EXPECT_FALSE(component::LuaScript::fromYaml("- not a map").has_value());
}
