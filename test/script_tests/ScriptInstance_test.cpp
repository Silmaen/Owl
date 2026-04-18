/**
 * @file ScriptInstance_test.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>

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

TEST(ScriptInstance, createAndLifecycle) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const auto dir = std::filesystem::temp_directory_path() / "owl_scriptinstance_test_1";
	std::filesystem::remove_all(dir);
	const auto path = writeTempScript(dir, "lifecycle.lua",
									  "created = false\n"
									  "updated = false\n"
									  "destroyed = false\n"
									  "last_dt = 0\n"
									  "function on_create()\n"
									  "  created = true\n"
									  "end\n"
									  "function on_update(dt)\n"
									  "  updated = true\n"
									  "  last_dt = dt\n"
									  "end\n"
									  "function on_destroy()\n"
									  "  destroyed = true\n"
									  "end\n");

	ScriptInstance inst;
	EXPECT_FALSE(inst.isValid());
	EXPECT_TRUE(inst.create(path.string(), 42));
	EXPECT_TRUE(inst.isValid());

	inst.onCreate();
	auto val = inst.getPropertyBool("created");
	ASSERT_TRUE(val.has_value());
	EXPECT_TRUE(val.value());

	inst.onUpdate(0.016f);
	val = inst.getPropertyBool("updated");
	ASSERT_TRUE(val.has_value());
	EXPECT_TRUE(val.value());
	auto dt = inst.getPropertyFloat("last_dt");
	ASSERT_TRUE(dt.has_value());
	EXPECT_NEAR(dt.value(), 0.016f, 0.001f);

	inst.onDestroy();
	val = inst.getPropertyBool("destroyed");
	ASSERT_TRUE(val.has_value());
	EXPECT_TRUE(val.value());

	ScriptEngine::shutdown();
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(ScriptInstance, createFromBuffer) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script =
			"buf_val = 0\n"
			"function on_create()\n"
			"  buf_val = 99\n"
			"end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	EXPECT_TRUE(inst.createFromBuffer(data, "buf_inst", 1));
	EXPECT_TRUE(inst.isValid());
	inst.onCreate();

	const auto val = inst.getPropertyInt("buf_val");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(val.value(), 99);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(ScriptInstance, propertyRoundTrip) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "speed = 0\nname = ''\nflag = false\ncount = 0\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "props", 1));

	inst.setProperty("speed", 3.14f);
	EXPECT_NEAR(inst.getPropertyFloat("speed").value_or(0.0f), 3.14f, 0.01f);

	inst.setProperty("name", std::string("hero"));
	EXPECT_EQ(inst.getPropertyString("name").value_or(""), "hero");

	inst.setProperty("flag", true);
	EXPECT_TRUE(inst.getPropertyBool("flag").value_or(false));

	inst.setProperty("count", static_cast<int64_t>(42));
	EXPECT_EQ(inst.getPropertyInt("count").value_or(0), 42);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(ScriptInstance, onCollision) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script =
			"collided_with = 0\n"
			"function on_collision(other_id)\n"
			"  collided_with = other_id\n"
			"end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "collision", 1));

	inst.onCollision(12345);
	const auto val = inst.getPropertyInt("collided_with");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(val.value(), 12345);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(ScriptInstance, missingCallbacksDoNotCrash) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "-- no callbacks defined\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "empty_script", 1));

	// These should not crash.
	inst.onCreate();
	inst.onUpdate(0.016f);
	inst.onDestroy();
	inst.onCollision(0);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(ScriptInstance, invalidInstanceOperations) {
	core::Log::init(core::Log::Level::Off);

	ScriptInstance inst;
	EXPECT_FALSE(inst.isValid());

	// These should not crash on invalid instance.
	inst.onCreate();
	inst.onUpdate(0.016f);
	inst.onDestroy();
	inst.onCollision(0);
	inst.setProperty("x", 1.0f);
	EXPECT_FALSE(inst.getPropertyFloat("x").has_value());

	core::Log::invalidate();
}

TEST(ScriptInstance, isolatedStates) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script1 = "counter = 0\nfunction on_create()\n  counter = 10\nend\n";
	const std::string script2 = "counter = 0\nfunction on_create()\n  counter = 20\nend\n";
	const std::vector<uint8_t> data1(script1.begin(), script1.end());
	const std::vector<uint8_t> data2(script2.begin(), script2.end());

	ScriptInstance inst1;
	ScriptInstance inst2;
	ASSERT_TRUE(inst1.createFromBuffer(data1, "script1", 1));
	ASSERT_TRUE(inst2.createFromBuffer(data2, "script2", 2));

	inst1.onCreate();
	inst2.onCreate();

	EXPECT_EQ(inst1.getPropertyInt("counter").value_or(0), 10);
	EXPECT_EQ(inst2.getPropertyInt("counter").value_or(0), 20);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}
