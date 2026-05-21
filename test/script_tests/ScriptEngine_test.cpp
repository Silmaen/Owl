/**
 * @file ScriptEngine_test.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Scene.h>
#include <script/ScriptEngine.h>

#include <fstream>

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

TEST(ScriptEngine, initAndShutdown) {
	core::Log::init(core::Log::Level::Off);
	EXPECT_FALSE(ScriptEngine::isInitialized());

	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	EXPECT_TRUE(ScriptEngine::isInitialized());
	EXPECT_EQ(ScriptEngine::getActiveScene(), scn.get());

	ScriptEngine::shutdown();
	EXPECT_FALSE(ScriptEngine::isInitialized());
	EXPECT_EQ(ScriptEngine::getActiveScene(), nullptr);

	core::Log::invalidate();
}

TEST(ScriptEngine, loadScript) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const auto dir = std::filesystem::temp_directory_path() / "owl_scriptengine_test_1";
	std::filesystem::remove_all(dir);
	const auto path = writeTempScript(dir, "engine_test.lua", "test_var = 123\n");

	EXPECT_TRUE(ScriptEngine::loadScript(path));

	ScriptEngine::shutdown();
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(ScriptEngine, loadScriptFromBuffer) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "buf_var = 456";
	const std::vector<uint8_t> data(script.begin(), script.end());
	EXPECT_TRUE(ScriptEngine::loadScriptFromBuffer(data, "buffer_test"));

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(ScriptEngine, loadScriptNotInitialized) {
	core::Log::init(core::Log::Level::Off);
	EXPECT_FALSE(ScriptEngine::loadScript("/nonexistent.lua"));
	const std::vector<uint8_t> data;
	EXPECT_FALSE(ScriptEngine::loadScriptFromBuffer(data, "empty"));
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
