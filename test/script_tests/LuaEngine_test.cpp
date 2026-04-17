/**
 * @file LuaEngine_test.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <script/LuaEngine.h>

#include <fstream>

using namespace owl::script;

namespace {

/// Helper to write a temporary Lua file for testing.
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

TEST(LuaEngine, creation) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	EXPECT_TRUE(engine.isValid());
	EXPECT_NE(engine.getState(), nullptr);
	owl::core::Log::invalidate();
}

TEST(LuaEngine, loadAndCallFunction) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const auto dir = std::filesystem::temp_directory_path() / "owl_luaengine_test_1";
	std::filesystem::remove_all(dir);
	const auto path = writeTempScript(dir, "test_basic.lua",
									  "function on_create()\n"
									  "  result = 42\n"
									  "end\n");

	EXPECT_TRUE(engine.loadScript(path));
	EXPECT_TRUE(engine.hasFunction("on_create"));
	EXPECT_FALSE(engine.hasFunction("nonexistent"));
	EXPECT_TRUE(engine.callFunction("on_create"));

	const auto val = engine.getGlobalInt("result");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(val.value(), 42);

	std::filesystem::remove_all(dir);
	owl::core::Log::invalidate();
}

TEST(LuaEngine, loadBuffer) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const std::string script = "greeting = 'hello from buffer'";
	const std::vector<uint8_t> data(script.begin(), script.end());

	EXPECT_TRUE(engine.loadBuffer(data, "test_buffer"));

	const auto val = engine.getGlobalString("greeting");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(val.value(), "hello from buffer");

	owl::core::Log::invalidate();
}

TEST(LuaEngine, syntaxError) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const std::string script = "this is not valid lua!!!";
	const std::vector<uint8_t> data(script.begin(), script.end());

	EXPECT_FALSE(engine.loadBuffer(data, "bad_syntax"));

	owl::core::Log::invalidate();
}

TEST(LuaEngine, runtimeError) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const std::string script = "function crashMe()\n  error('boom')\nend\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	ASSERT_TRUE(engine.loadBuffer(data, "runtime_error"));

	EXPECT_FALSE(engine.callFunction("crashMe"));

	owl::core::Log::invalidate();
}

TEST(LuaEngine, callFunctionWithFloatArg) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const std::string script = "function on_update(dt)\n  delta = dt\nend\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	ASSERT_TRUE(engine.loadBuffer(data, "float_arg"));

	EXPECT_TRUE(engine.callFunction("on_update", 0.016f));

	const auto val = engine.getGlobalFloat("delta");
	ASSERT_TRUE(val.has_value());
	EXPECT_NEAR(val.value(), 0.016f, 0.001f);

	owl::core::Log::invalidate();
}

TEST(LuaEngine, callFunctionWithIntArg) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const std::string script = "function on_collision(otherId)\n  collider = otherId\nend\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	ASSERT_TRUE(engine.loadBuffer(data, "int_arg"));

	EXPECT_TRUE(engine.callFunction("on_collision", static_cast<uint64_t>(12345)));

	const auto val = engine.getGlobalInt("collider");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(val.value(), 12345);

	owl::core::Log::invalidate();
}

TEST(LuaEngine, callNonExistentFunction) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	EXPECT_FALSE(engine.callFunction("nope"));
	EXPECT_FALSE(engine.callFunction("nope", 1.0f));
	EXPECT_FALSE(engine.callFunction("nope", static_cast<uint64_t>(0)));

	owl::core::Log::invalidate();
}

TEST(LuaEngine, globalGetSet) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// Float
	engine.setGlobal("myFloat", 3.14f);
	auto fVal = engine.getGlobalFloat("myFloat");
	ASSERT_TRUE(fVal.has_value());
	EXPECT_NEAR(fVal.value(), 3.14f, 0.01f);

	// Int
	engine.setGlobal("myInt", static_cast<int64_t>(99));
	auto iVal = engine.getGlobalInt("myInt");
	ASSERT_TRUE(iVal.has_value());
	EXPECT_EQ(iVal.value(), 99);

	// String
	engine.setGlobal("myStr", std::string("hello"));
	auto sVal = engine.getGlobalString("myStr");
	ASSERT_TRUE(sVal.has_value());
	EXPECT_EQ(sVal.value(), "hello");

	// Bool
	engine.setGlobal("myBool", true);
	auto bVal = engine.getGlobalBool("myBool");
	ASSERT_TRUE(bVal.has_value());
	EXPECT_TRUE(bVal.value());

	engine.setGlobal("myBool", false);
	bVal = engine.getGlobalBool("myBool");
	ASSERT_TRUE(bVal.has_value());
	EXPECT_FALSE(bVal.value());

	owl::core::Log::invalidate();
}

TEST(LuaEngine, wrongTypeReturnsNullopt) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	engine.setGlobal("aString", std::string("text"));

	EXPECT_FALSE(engine.getGlobalBool("aString").has_value());
	EXPECT_FALSE(engine.getGlobalInt("aString").has_value());
	// Note: Lua numbers and strings are interconvertible, so getGlobalFloat on a numeric string may succeed.
	// getGlobalBool on a non-boolean should fail.
	EXPECT_FALSE(engine.getGlobalBool("nonexistent").has_value());
	EXPECT_FALSE(engine.getGlobalFloat("nonexistent").has_value());
	EXPECT_FALSE(engine.getGlobalInt("nonexistent").has_value());
	EXPECT_FALSE(engine.getGlobalString("nonexistent").has_value());

	owl::core::Log::invalidate();
}

TEST(LuaEngine, sandboxing) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// io library should not be available.
	const std::string scriptIo = "result = (io ~= nil)";
	const std::vector<uint8_t> dataIo(scriptIo.begin(), scriptIo.end());
	ASSERT_TRUE(engine.loadBuffer(dataIo, "sandbox_io"));
	auto val = engine.getGlobalBool("result");
	ASSERT_TRUE(val.has_value());
	EXPECT_FALSE(val.value());

	// os library should not be available.
	LuaEngine engine2;
	const std::string scriptOs = "result = (os ~= nil)";
	const std::vector<uint8_t> dataOs(scriptOs.begin(), scriptOs.end());
	ASSERT_TRUE(engine2.loadBuffer(dataOs, "sandbox_os"));
	val = engine2.getGlobalBool("result");
	ASSERT_TRUE(val.has_value());
	EXPECT_FALSE(val.value());

	// dofile should not be available.
	LuaEngine engine3;
	const std::string scriptDofile = "result = (dofile ~= nil)";
	const std::vector<uint8_t> dataDofile(scriptDofile.begin(), scriptDofile.end());
	ASSERT_TRUE(engine3.loadBuffer(dataDofile, "sandbox_dofile"));
	val = engine3.getGlobalBool("result");
	ASSERT_TRUE(val.has_value());
	EXPECT_FALSE(val.value());

	// math library SHOULD be available.
	LuaEngine engine4;
	const std::string scriptMath = "result = (math ~= nil)";
	const std::vector<uint8_t> dataMath(scriptMath.begin(), scriptMath.end());
	ASSERT_TRUE(engine4.loadBuffer(dataMath, "sandbox_math"));
	val = engine4.getGlobalBool("result");
	ASSERT_TRUE(val.has_value());
	EXPECT_TRUE(val.value());

	owl::core::Log::invalidate();
}

TEST(LuaEngine, loadNonexistentFile) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	EXPECT_FALSE(engine.loadScript("/nonexistent/path/to/script.lua"));

	owl::core::Log::invalidate();
}
