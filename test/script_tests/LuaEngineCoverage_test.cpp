/**
 * @file LuaEngineCoverage_test.cpp
 * @author Silmaen
 * @date 14/04/2026
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

TEST(LuaEngineCoverage, loadfileIsBlocked) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// loadfile should have been removed by sandboxing.
	const std::string script = "result = (loadfile ~= nil)";
	const std::vector<uint8_t> data(script.begin(), script.end());
	ASSERT_TRUE(engine.loadBuffer(data, "sandbox_loadfile"));
	const auto val = engine.getGlobalBool("result");
	ASSERT_TRUE(val.has_value());
	EXPECT_FALSE(val.value());

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, ioLibIsBlocked) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// Attempting to use io should fail.
	const std::string script = "function tryIo()\n  local f = io.open('/tmp/test', 'r')\nend\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	ASSERT_TRUE(engine.loadBuffer(data, "io_test"));
	// Calling the function that uses io should produce an error.
	EXPECT_FALSE(engine.callFunction("tryIo"));

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, osLibIsBlocked) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// Attempting to use os should fail.
	const std::string script = "function tryOs()\n  return os.clock()\nend\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	ASSERT_TRUE(engine.loadBuffer(data, "os_test"));
	EXPECT_FALSE(engine.callFunction("tryOs"));

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, safeLibsAvailable) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// table library should be available.
	{
		const std::string script = "result = (table ~= nil)";
		const std::vector<uint8_t> data(script.begin(), script.end());
		ASSERT_TRUE(engine.loadBuffer(data, "table_check"));
		const auto val = engine.getGlobalBool("result");
		ASSERT_TRUE(val.has_value());
		EXPECT_TRUE(val.value());
	}

	// string library should be available.
	{
		LuaEngine engine2;
		const std::string script = "result = (string ~= nil)";
		const std::vector<uint8_t> data(script.begin(), script.end());
		ASSERT_TRUE(engine2.loadBuffer(data, "string_check"));
		const auto val = engine2.getGlobalBool("result");
		ASSERT_TRUE(val.has_value());
		EXPECT_TRUE(val.value());
	}

	// utf8 library should be available.
	{
		LuaEngine engine3;
		const std::string script = "result = (utf8 ~= nil)";
		const std::vector<uint8_t> data(script.begin(), script.end());
		ASSERT_TRUE(engine3.loadBuffer(data, "utf8_check"));
		const auto val = engine3.getGlobalBool("result");
		ASSERT_TRUE(val.has_value());
		EXPECT_TRUE(val.value());
	}

	// coroutine library should be available.
	{
		LuaEngine engine4;
		const std::string script = "result = (coroutine ~= nil)";
		const std::vector<uint8_t> data(script.begin(), script.end());
		ASSERT_TRUE(engine4.loadBuffer(data, "coroutine_check"));
		const auto val = engine4.getGlobalBool("result");
		ASSERT_TRUE(val.has_value());
		EXPECT_TRUE(val.value());
	}

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, loadBufferSyntaxError) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// Syntax error in buffer.
	const std::string script = "function bad(\nend end end";
	const std::vector<uint8_t> data(script.begin(), script.end());
	EXPECT_FALSE(engine.loadBuffer(data, "syntax_error_buf"));

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, loadBufferRuntimeError) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// Runtime error during chunk execution (top level code that errors).
	const std::string script = "error('top level boom')";
	const std::vector<uint8_t> data(script.begin(), script.end());
	EXPECT_FALSE(engine.loadBuffer(data, "runtime_error_buf"));

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, callFunctionWithFloatRuntimeError) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const std::string script = "function crashFloat(dt)\n  error('float boom')\nend\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	ASSERT_TRUE(engine.loadBuffer(data, "float_crash"));
	EXPECT_FALSE(engine.callFunction("crashFloat", 0.016f));

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, callFunctionWithIntRuntimeError) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const std::string script = "function crashInt(id)\n  error('int boom')\nend\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	ASSERT_TRUE(engine.loadBuffer(data, "int_crash"));
	EXPECT_FALSE(engine.callFunction("crashInt", static_cast<uint64_t>(42)));

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, emptyBuffer) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// Empty buffer should load successfully (empty chunk).
	const std::vector<uint8_t> data;
	EXPECT_TRUE(engine.loadBuffer(data, "empty"));

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, loadScriptBadSyntax) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	const auto dir = owl::test::getRootPath() / "output" / "test_tmp_lua_cov";
	const auto path = writeTempScript(dir, "bad_syntax.lua", "function bad(\nend end end");
	EXPECT_FALSE(engine.loadScript(path));

	std::filesystem::remove_all(dir);
	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, getGlobalWrongTypes) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	LuaEngine engine;
	ASSERT_TRUE(engine.isValid());

	// Set an integer and try to read it as other types.
	engine.setGlobal("anInt", static_cast<int64_t>(42));
	EXPECT_FALSE(engine.getGlobalBool("anInt").has_value());
	// In Lua 5.5, an integer is not a float (lua_isnumber returns true for both,
	// but we test specific type mismatches via bool).

	// Set a boolean and try to read it as other types.
	engine.setGlobal("aBool", true);
	EXPECT_FALSE(engine.getGlobalInt("aBool").has_value());
	EXPECT_FALSE(engine.getGlobalFloat("aBool").has_value());
	EXPECT_FALSE(engine.getGlobalString("aBool").has_value());

	owl::core::Log::invalidate();
}

TEST(LuaEngineCoverage, multipleScriptsIsolation) {
	owl::core::Log::init(owl::core::Log::Level::Off);

	// Two engines should be isolated.
	LuaEngine engine1;
	LuaEngine engine2;
	ASSERT_TRUE(engine1.isValid());
	ASSERT_TRUE(engine2.isValid());

	engine1.setGlobal("shared_var", static_cast<int64_t>(111));
	engine2.setGlobal("shared_var", static_cast<int64_t>(222));

	const auto val1 = engine1.getGlobalInt("shared_var");
	const auto val2 = engine2.getGlobalInt("shared_var");
	ASSERT_TRUE(val1.has_value());
	ASSERT_TRUE(val2.has_value());
	EXPECT_EQ(val1.value(), 111);
	EXPECT_EQ(val2.value(), 222);

	owl::core::Log::invalidate();
}
