/**
 * @file LuaMods_test.cpp
 * @author Silmaen
 * @date 16/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/ScriptedComponents.h>
#include <script/ScriptInstance.h>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

constexpr auto g_Manifest = "OwlExtension:\n  id: rules\n  kind: Script\n  script: scripts/rules/main.lua\n";

constexpr auto g_Script = R"(
components = {
	["rules.Health"] = {
		{ name = "hp", type = "int", default = 100, min = 0 },
		{ name = "spawn", type = "vec2", default = { 1, 2 } },
	},
	["other.Stolen"] = { { name = "x", type = "float" } },
}
loaded = false
spawned = 0
destroyed = 0
read_back = 0
function on_scene_load()
	loaded = true
end
function on_entity_spawn(id)
	spawned = spawned + 1
	entity.add_component(id, "rules.Health")
	entity.set_field(id, "rules.Health", "hp", 42)
	entity.set_field(id, "rules.Health", "spawn", { x = 3, y = 4 })
	read_back = entity.get_field(id, "rules.Health", "hp")
	wrong_type = entity.set_field(id, "rules.Health", "hp", "many")
	no_field = entity.get_field(id, "rules.Health", "absent") == nil
	has = entity.has_component(id, "rules.Health")
end
function on_entity_destroy(id)
	destroyed = destroyed + 1
end
)";

void writeText(const std::filesystem::path& iFile, const std::string& iText) {
	std::filesystem::create_directories(iFile.parent_path());
	std::ofstream file(iFile, std::ios::binary);
	file << iText;
}

class LuaMods : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
		m_dir = std::filesystem::temp_directory_path() / "owl_lua_mods_test";
		std::filesystem::remove_all(m_dir);
		writeText(m_dir / "rules" / "extension.yml", g_Manifest);
		writeText(m_dir / "rules" / "scripts" / "rules" / "main.lua", g_Script);
	}

	void TearDown() override {
		std::ignore = m_app->setExtensionSources({});
		std::filesystem::remove_all(m_dir);
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	shared<app::Application> m_app;
	std::filesystem::path m_dir;
};

}// namespace

TEST_F(LuaMods, componentsOfTheEntryScriptAreRegisteredWhileMounted) {
	ASSERT_EQ(m_app->setExtensionSources({m_dir / "rules"}), 1u);
	ASSERT_EQ(ScriptedComponents::getTypes().size(), 1u);
	const auto* type = ScriptedComponents::findType("rules.Health");
	ASSERT_NE(type, nullptr);
	EXPECT_EQ(type->extensionId, "rules");
	EXPECT_EQ(type->fields.size(), 2u);
	EXPECT_EQ(ScriptedComponents::findType("other.Stolen"), nullptr);
	const auto* descriptor = ComponentRegistry::find("rules.Health");
	ASSERT_NE(descriptor, nullptr);
	EXPECT_FALSE(descriptor->builtin);
	EXPECT_TRUE(descriptor->optional);

	EXPECT_FALSE(ScriptedComponents::registerType({.key = "rules.Health", .extensionId = "thief", .fields = {}}));
	EXPECT_FALSE(ScriptedComponents::registerType({.key = "Transform", .extensionId = "", .fields = {}}));
	EXPECT_FALSE(ScriptedComponents::registerType({.key = "rules.", .extensionId = "rules", .fields = {}}));
	EXPECT_FALSE(ScriptedComponents::unregisterType("Transform"));

	std::ignore = m_app->setExtensionSources({});
	EXPECT_TRUE(ScriptedComponents::getTypes().empty());
	EXPECT_EQ(ComponentRegistry::find("rules.Health"), nullptr);
}

TEST_F(LuaMods, scriptedComponentIsSavedCopiedAndDuplicated) {
	ASSERT_EQ(m_app->setExtensionSources({m_dir / "rules"}), 1u);
	const auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Knight");
	EXPECT_EQ(ScriptedComponents::get(entity, "rules.Health"), nullptr);
	EXPECT_EQ(ScriptedComponents::add(entity, "rules.None"), nullptr);
	auto* values = ScriptedComponents::add(entity, "rules.Health");
	ASSERT_NE(values, nullptr);
	ASSERT_NE(values->find("hp"), nullptr);
	EXPECT_EQ(std::get<int64_t>(values->find("hp")->value), 100);
	values->find("hp")->value = int64_t{7};
	EXPECT_EQ(values->find("absent"), nullptr);
	EXPECT_EQ(std::as_const(*values).find("absent"), nullptr);

	const SceneSerializer serializer(scene);
	const auto yaml = serializer.serializeToString();
	EXPECT_NE(yaml.find("rules.Health"), std::string::npos);
	const auto loaded = mkShared<Scene>();
	const SceneSerializer loader(loaded);
	ASSERT_TRUE(loader.deserializeFromBuffer({yaml.begin(), yaml.end()}).has_value());
	const auto copy = loaded->findEntityByUUID(entity.getUUID());
	ASSERT_TRUE(copy);
	const auto* read = ScriptedComponents::get(copy, "rules.Health");
	ASSERT_NE(read, nullptr);
	EXPECT_EQ(std::get<int64_t>(read->find("hp")->value), 7);
	EXPECT_EQ((std::get<std::array<float, 2>>(read->find("spawn")->value)), (std::array<float, 2>{1.f, 2.f}));

	const auto duplicate = scene->duplicateEntity(entity);
	ASSERT_NE(ScriptedComponents::get(duplicate, "rules.Health"), nullptr);
	EXPECT_EQ(std::get<int64_t>(ScriptedComponents::get(duplicate, "rules.Health")->find("hp")->value), 7);
	const auto playCopy = Scene::copy(scene);
	const auto inCopy = playCopy->findEntityByUUID(entity.getUUID());
	ASSERT_NE(ScriptedComponents::get(inCopy, "rules.Health"), nullptr);

	const auto* descriptor = ComponentRegistry::find("rules.Health");
	ASSERT_NE(descriptor, nullptr);
	descriptor->remove(entity);
	EXPECT_FALSE(descriptor->has(entity));
	descriptor->add(entity);
	EXPECT_EQ(std::get<int64_t>(ScriptedComponents::get(entity, "rules.Health")->find("hp")->value), 100);
}

TEST_F(LuaMods, entryScriptReceivesTheGameEvents) {
	ASSERT_EQ(m_app->setExtensionSources({m_dir / "rules"}), 1u);
	const auto scene = mkShared<Scene>();
	scene->onStartRuntime();
	ASSERT_EQ(scene->getModScripts().size(), 1u);
	const auto& mod = *scene->getModScripts().front().instance;
	EXPECT_EQ(mod.getPropertyBool("loaded"), true);

	auto spawned = scene->createEntity("Spawned");
	scene->startSpawnedSubtree(spawned);
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds{16});
	scene->onUpdateRuntime(step, false);
	EXPECT_EQ(mod.getPropertyInt("spawned"), 1);
	EXPECT_EQ(mod.getPropertyInt("read_back"), 42);
	EXPECT_EQ(mod.getPropertyBool("wrong_type"), false);
	EXPECT_EQ(mod.getPropertyBool("no_field"), true);
	EXPECT_EQ(mod.getPropertyBool("has"), true);
	const auto* health = ScriptedComponents::get(spawned, "rules.Health");
	ASSERT_NE(health, nullptr);
	EXPECT_EQ(std::get<int64_t>(health->find("hp")->value), 42);
	EXPECT_EQ((std::get<std::array<float, 2>>(health->find("spawn")->value)), (std::array<float, 2>{3.f, 4.f}));

	scene->destroyEntityDeferred(spawned);
	scene->onUpdateRuntime(step, false);
	EXPECT_EQ(mod.getPropertyInt("destroyed"), 1);
	scene->onEndRuntime();
	EXPECT_TRUE(scene->getModScripts().empty());
}

TEST_F(LuaMods, dataPacksAndPacksWithoutScriptRunNothing) {
	writeText(m_dir / "data" / "extension.yml", "OwlExtension:\n  id: data\n  script: scripts/rules/main.lua\n");
	writeText(m_dir / "data" / "scripts" / "rules" / "main.lua", g_Script);
	writeText(m_dir / "noscript" / "extension.yml", "OwlExtension:\n  id: noscript\n  kind: Script\n");
	ASSERT_EQ(m_app->setExtensionSources({m_dir / "data", m_dir / "noscript"}), 2u);
	EXPECT_TRUE(ScriptedComponents::getTypes().empty());
	const auto scene = mkShared<Scene>();
	scene->onStartRuntime();
	EXPECT_TRUE(scene->getModScripts().empty());
	scene->onEndRuntime();
}
