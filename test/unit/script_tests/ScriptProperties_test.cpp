/**
 * @file ScriptProperties_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/LuaScript.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

constexpr auto g_declaration = "properties = {\n"
							   "  { name = 'speed', type = 'float', default = 5, min = 0, max = 20, step = 0.5 },\n"
							   "  { name = 'tint', type = 'color', default = {1, 0.5, 0.25, 1} },\n"
							   "  { name = 'offset', type = 'vec2', default = {1, 2} },\n"
							   "  { name = 'spawn', type = 'vec3', default = {1, 2, 3}, min = -10 },\n"
							   "  { name = 'target', type = 'entity' },\n"
							   "  { name = 'waypoints', type = 'numbers', default = {4, 5, 6} },\n"
							   "  { name = 'names', type = 'strings', default = {'a', 'b'} },\n"
							   "}\n";

auto buffer(const std::string& iText) -> std::vector<uint8_t> { return {iText.begin(), iText.end()}; }

auto find(const std::vector<ScriptProperty>& iProperties, const std::string& iName) -> const ScriptProperty& {
	for (const auto& property: iProperties)
		if (property.name == iName)
			return property;
	static const ScriptProperty none;
	return none;
}

}// namespace

TEST(ScriptProperties, ExtractionReadsTypesDefaultsAndRanges) {
	core::Log::init(core::Log::Level::Off);
	const auto properties = ScriptEngine::extractPropertiesFromBuffer(buffer(g_declaration), "declaration");
	ASSERT_EQ(properties.size(), 7u);
	const auto& speed = find(properties, "speed");
	EXPECT_EQ(speed.type, ScriptPropertyType::Float);
	EXPECT_FLOAT_EQ(std::get<float>(speed.defaultValue), 5.f);
	EXPECT_EQ(speed.min, 0.f);
	EXPECT_EQ(speed.max, 20.f);
	EXPECT_EQ(speed.step, 0.5f);
	const auto& tint = find(properties, "tint");
	EXPECT_EQ(tint.type, ScriptPropertyType::Color);
	EXPECT_EQ((std::get<std::array<float, 4>>(tint.value)), (std::array<float, 4>{1.f, 0.5f, 0.25f, 1.f}));
	EXPECT_EQ((std::get<std::array<float, 2>>(find(properties, "offset").value)), (std::array<float, 2>{1.f, 2.f}));
	const auto& spawn = find(properties, "spawn");
	EXPECT_EQ((std::get<std::array<float, 3>>(spawn.value)), (std::array<float, 3>{1.f, 2.f, 3.f}));
	EXPECT_EQ(spawn.min, -10.f);
	EXPECT_FALSE(spawn.max.has_value());
	EXPECT_EQ(std::get<uint64_t>(find(properties, "target").value), 0u);
	EXPECT_EQ((std::get<std::vector<double>>(find(properties, "waypoints").value)), (std::vector<double>{4., 5., 6.}));
	EXPECT_EQ((std::get<std::vector<std::string>>(find(properties, "names").value)),
			  (std::vector<std::string>{"a", "b"}));
	core::Log::invalidate();
}

TEST(ScriptProperties, TypeNamesRoundTrip) {
	for (const auto type:
		 {ScriptPropertyType::Float, ScriptPropertyType::Int, ScriptPropertyType::String, ScriptPropertyType::Bool,
		  ScriptPropertyType::Color, ScriptPropertyType::Vec2, ScriptPropertyType::Vec3, ScriptPropertyType::Entity,
		  ScriptPropertyType::Numbers, ScriptPropertyType::Strings}) {
		EXPECT_EQ(ScriptEngine::parsePropertyType(ScriptEngine::propertyTypeName(type)), type);
		const auto value = ScriptEngine::defaultValueFor(type);
		EXPECT_EQ(value.index() == ScriptEngine::defaultValueFor(type).index(), true);
	}
	EXPECT_FALSE(ScriptEngine::parsePropertyType("matrix").has_value());
}

TEST(ScriptProperties, TheSceneFileKeepsEveryType) {
	core::Log::init(core::Log::Level::Off);
	scene::component::LuaScript script;
	script.scriptPath = "scripts/a.lua";
	script.properties = ScriptEngine::extractPropertiesFromBuffer(buffer(g_declaration), "declaration");
	std::get<std::array<float, 4>>(script.properties[1].value) = {0.f, 1.f, 0.f, 0.5f};
	script.properties[4].value = uint64_t{123456789012ULL};
	const auto back = scene::component::LuaScript::fromYaml(script.toYaml());
	ASSERT_TRUE(back.has_value());
	ASSERT_EQ(back->properties.size(), script.properties.size());
	for (size_t index = 0; index < script.properties.size(); ++index) {
		EXPECT_EQ(back->properties[index].name, script.properties[index].name);
		EXPECT_EQ(back->properties[index].type, script.properties[index].type);
		EXPECT_EQ(back->properties[index].value, script.properties[index].value) << script.properties[index].name;
	}
	core::Log::invalidate();
}

TEST(ScriptProperties, LuaReadsTheValuesAsTablesAndIntegers) {
	core::Log::init(core::Log::Level::Off);
	auto scene = mkShared<scene::Scene>();
	const ScriptInstance instance;
	instance.setScene(scene.get());
	const std::string source = std::string{g_declaration} + "function on_create()\n"
															"  gamestate.set('g', tint.g)\n"
															"  gamestate.set('y', offset.y)\n"
															"  gamestate.set('z', spawn.z)\n"
															"  gamestate.set('target', target)\n"
															"  gamestate.set('wp', waypoints[2] + #waypoints)\n"
															"  gamestate.set('names', names[2])\n"
															"end\n";
	ASSERT_TRUE(instance.createFromBuffer(buffer(source), "props", 1));
	for (auto property: ScriptEngine::extractPropertiesFromBuffer(buffer(source), "props")) {
		if (property.name == "target")
			property.value = uint64_t{42};
		instance.setProperty(property);
	}
	instance.onCreate();
	const auto read = [&scene](const char* iKey) -> script::ScriptValue {
		const auto value = scene->getGameState().get(iKey);
		return value ? std::visit(
							   [](const auto& iAlt) -> script::ScriptValue {
								   using Alt = std::decay_t<decltype(iAlt)>;
								   if constexpr (std::is_same_v<Alt, double> || std::is_same_v<Alt, float>)
									   return static_cast<float>(iAlt);
								   else if constexpr (std::is_same_v<Alt, int64_t> || std::is_same_v<Alt, std::string>)
									   return iAlt;
								   else
									   return 0.f;
							   },
							   *value)
					 : script::ScriptValue{-1.f};
	};
	EXPECT_FLOAT_EQ(std::get<float>(read("g")), 0.5f);
	EXPECT_FLOAT_EQ(std::get<float>(read("y")), 2.f);
	EXPECT_FLOAT_EQ(std::get<float>(read("z")), 3.f);
	EXPECT_EQ(std::get<int64_t>(read("target")), 42);
	EXPECT_FLOAT_EQ(std::get<float>(read("wp")), 8.f);
	EXPECT_EQ(std::get<std::string>(read("names")), "b");
	core::Log::invalidate();
}

TEST(ScriptProperties, APropertyTheSceneDoesNotListTakesTheDeclaredDefaultAtRuntime) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_script_property_defaults";
	std::filesystem::create_directories(dir);
	{
		std::ofstream file{dir / "tuned.lua"};
		file << "properties = {\n  { name = 'speed', type = 'float', default = 5 },\n"
				"  { name = 'lives', type = 'int', default = 3 },\n}\n"
				"function on_create()\n  gamestate.set('speed', speed)\n  gamestate.set('lives', lives)\nend\n";
	}
	auto scene = mkShared<scene::Scene>();
	auto& component = scene->createEntity("Tuned").addComponent<scene::component::LuaScript>();
	component.scriptPath = (dir / "tuned.lua").string();
	component.properties.push_back({.name = "lives", .type = ScriptPropertyType::Int, .value = int64_t{9}});
	scene->onStartRuntime();
	const auto speed = scene->getGameState().get("speed");
	const auto lives = scene->getGameState().get("lives");
	ASSERT_TRUE(speed.has_value());
	ASSERT_TRUE(lives.has_value());
	EXPECT_FLOAT_EQ(std::get<float>(*speed), 5.f);
	EXPECT_EQ(std::get<int64_t>(*lives), 9);
	scene->onEndRuntime();
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
