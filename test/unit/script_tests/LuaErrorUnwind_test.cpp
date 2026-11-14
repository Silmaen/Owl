/**
 * @file LuaErrorUnwind_test.cpp
 * @author Silmaen
 * @date 12/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/external/lua.h>
#include <script/LuaBindings.h>
#include <script/LuaEngine.h>

#include <cstddef>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

/// Number of `Guard` objects destroyed since the fixture started.
int g_destroyed = 0;

/// RAII object a binding holds while it raises a Lua error.
class Guard final {
public:
	Guard() = default;
	Guard(const Guard&) = delete;
	Guard(Guard&&) = delete;
	auto operator=(const Guard&) -> Guard& = delete;
	auto operator=(Guard&&) -> Guard& = delete;
	~Guard() { ++g_destroyed; }
};

// Holds heap objects and a guard, then fails its argument check.
auto checkAfterObjects(lua_State* iState) -> int {
	const Guard guard;
	const std::string text(256, 'x');
	const std::vector<double> values(64, 1.0);
	const double number = luaL_checknumber(iState, 1);
	lua_pushnumber(iState, number + static_cast<double>(text.size() + values.size()));
	return 1;
}

// Holds a guard and raises a formatted Lua error.
[[noreturn]] auto errorAfterGuard(lua_State* iState) -> int {
	const Guard guard;
	const std::string what(128, 'y');
	luaL_error(iState, "binding failed after %d bytes", static_cast<int>(what.size()));
}

// Holds a guard and calls the Lua function given as argument, whose error goes through the binding.
auto callThrough(lua_State* iState) -> int {
	luaL_checktype(iState, 1, LUA_TFUNCTION);
	const Guard guard;
	lua_pushvalue(iState, 1);
	lua_call(iState, 0, 0);
	return 0;
}

constexpr luaL_Reg g_functions[] = {{"check", checkAfterObjects},
									{"fail", errorAfterGuard},
									{"call", callThrough},
									{nullptr, nullptr}};

class LuaErrorUnwind : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		g_destroyed = 0;
		LuaEngine::registerGuardedTable(engine.getState(), "raii", g_functions);
	}

	void TearDown() override { core::Log::invalidate(); }

	/// Engine with a roomy time quota: throwing is slow under an emulator.
	LuaEngine engine{ScriptQuotas{.memoryBytes = size_t{8} * 1024 * 1024, .timePerCallMs = 5000}};
};

}// namespace

TEST_F(LuaErrorUnwind, argumentCheckRunsTheDestructors) {
	const auto result = engine.runChunk("return raii.check('not a number')", "unwind_test");
	EXPECT_TRUE(result.compiled);
	EXPECT_FALSE(result.ok);
	EXPECT_NE(result.message.find("invalid argument #1"), std::string::npos) << result.message;
	EXPECT_EQ(g_destroyed, 1);
	const auto passed = engine.runChunk("return raii.check(2)", "unwind_test");
	ASSERT_TRUE(passed.ok) << passed.message;
	ASSERT_EQ(passed.values.size(), 1u);
	EXPECT_EQ(passed.values.front(), "322");
	EXPECT_EQ(g_destroyed, 2);
}

TEST_F(LuaErrorUnwind, raisedErrorReachesTheCallbackErrorPath) {
	const std::string script{"function on_update() raii.fail() end\n"};
	ASSERT_TRUE(engine.loadBuffer({script.begin(), script.end()}, "unwind_test"));
	EXPECT_FALSE(engine.callFunction("on_update"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::RuntimeError);
	EXPECT_EQ(g_destroyed, 1);
	const auto result = engine.runChunk("raii.fail()", "unwind_test");
	EXPECT_NE(result.message.find("binding failed after 128 bytes"), std::string::npos) << result.message;
	EXPECT_EQ(g_destroyed, 2);
}

TEST_F(LuaErrorUnwind, scriptErrorThroughABindingKeepsItsObject) {
	const auto result = engine.runChunk("local marker = {}\n"
										"local ok, err = pcall(raii.call, function() error(marker) end)\n"
										"return not ok and err == marker\n",
										"unwind_test");
	ASSERT_TRUE(result.ok) << result.message;
	ASSERT_EQ(result.values.size(), 1u);
	EXPECT_EQ(result.values.front(), "true");
	EXPECT_EQ(g_destroyed, 1);
}

TEST_F(LuaErrorUnwind, errorInsideACoroutineRunsTheDestructors) {
	const auto result = engine.runChunk("local co = coroutine.create(function() raii.check({}) end)\n"
										"local ok, err = coroutine.resume(co)\n"
										"return not ok and string.find(err, 'invalid argument', 1, true) ~= nil\n",
										"unwind_test");
	ASSERT_TRUE(result.ok) << result.message;
	ASSERT_EQ(result.values.size(), 1u);
	EXPECT_EQ(result.values.front(), "true");
	EXPECT_EQ(g_destroyed, 1);
}

TEST_F(LuaErrorUnwind, engineBindingCheckAfterAStringNamesTheArgument) {
	registerBindings(engine.getState());
	const auto result = engine.runChunk(
			"scene.transition_to('scenes/" + std::string(200, 'a') + ".owl', 'fade', 1, 1, 0, {})", "unwind_test");
	EXPECT_FALSE(result.ok);
	EXPECT_NE(result.message.find("invalid argument #6 to 'transition_to'"), std::string::npos) << result.message;
}
