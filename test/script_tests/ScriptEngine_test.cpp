/**
 * @file ScriptEngine_test.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Transform.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
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

TEST(ScriptEngine, InstancesActOnTheirOwnScene) {
	core::Log::init(core::Log::Level::Off);
	scene::Scene first;
	scene::Scene second;
	const auto inFirst = first.createEntity("Probe");
	const auto inSecond = second.createEntity("Probe");
	const std::string script = "function on_create()\n"
							   "  transform.set_position(entity_id, 4.0, 5.0, 6.0)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance firstScript;
	firstScript.setScene(&first);
	EXPECT_EQ(firstScript.getScene(), &first);
	ASSERT_TRUE(firstScript.createFromBuffer(data, "first", static_cast<uint64_t>(inFirst.getUUID())));
	const ScriptInstance secondScript;
	ASSERT_TRUE(secondScript.createFromBuffer(data, "second", static_cast<uint64_t>(inFirst.getUUID())));
	secondScript.setScene(&second);
	firstScript.onCreate();
	secondScript.onCreate();
	EXPECT_FLOAT_EQ(inFirst.getComponent<scene::component::Transform>().transform.translation().x(), 4.f);
	// The second script targets the UUID of the first scene's entity: its own scene has no such entity.
	EXPECT_FLOAT_EQ(inSecond.getComponent<scene::component::Transform>().transform.translation().x(), 0.f);
	core::Log::invalidate();
}

TEST(ScriptEngine, extractProperties) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_scriptengine_test_2";
	std::filesystem::remove_all(dir);
	const auto path = writeTempScript(dir, "props_test.lua",
									  "properties = {\n"
									  "  { name = 'speed', type = 'float', default = 5.0 },\n"
									  "  { name = 'health', type = 'int', default = 100 },\n"
									  "  { name = 'label', type = 'string', default = 'player' },\n"
									  "  { name = 'active', type = 'bool', default = true },\n"
									  "}\n");

	const auto props = ScriptEngine::extractProperties(path);
	EXPECT_EQ(props.size(), 4);

	// Properties may come in any order from Lua table iteration, so find by name.
	for (const auto& prop: props) {
		if (prop.name == "speed") {
			EXPECT_EQ(prop.type, ScriptPropertyType::Float);
			EXPECT_NEAR(std::get<float>(prop.value), 5.0f, 0.01f);
		} else if (prop.name == "health") {
			EXPECT_EQ(prop.type, ScriptPropertyType::Int);
			EXPECT_EQ(std::get<int64_t>(prop.value), 100);
		} else if (prop.name == "label") {
			EXPECT_EQ(prop.type, ScriptPropertyType::String);
			EXPECT_EQ(std::get<std::string>(prop.value), "player");
		} else if (prop.name == "active") {
			EXPECT_EQ(prop.type, ScriptPropertyType::Bool);
			EXPECT_TRUE(std::get<bool>(prop.value));
		}
	}

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(ScriptEngine, extractPropertiesFromBuffer) {
	core::Log::init(core::Log::Level::Off);

	const std::string script = "properties = {\n"
							   "  { name = 'damage', type = 'float', default = 10.0 },\n"
							   "}\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const auto props = ScriptEngine::extractPropertiesFromBuffer(data, "buf_props");
	EXPECT_EQ(props.size(), 1);
	EXPECT_EQ(props[0].name, "damage");
	EXPECT_EQ(props[0].type, ScriptPropertyType::Float);
	EXPECT_NEAR(std::get<float>(props[0].value), 10.0f, 0.01f);

	core::Log::invalidate();
}

TEST(ScriptEngine, extractPropertiesNoTable) {
	core::Log::init(core::Log::Level::Off);

	const std::string script = "-- no properties table\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const auto props = ScriptEngine::extractPropertiesFromBuffer(data, "no_props");
	EXPECT_TRUE(props.empty());

	core::Log::invalidate();
}
