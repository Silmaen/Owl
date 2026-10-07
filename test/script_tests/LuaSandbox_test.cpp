/**
 * @file LuaSandbox_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/external/lua.h>
#include <cstdio>
#include <format>
#include <scene/Scene.h>
#include <script/LuaBindings.h>
#include <script/LuaEngine.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

auto toBuffer(const std::string& iText) -> std::vector<uint8_t> { return {iText.begin(), iText.end()}; }

constexpr ScriptQuotas g_SmallQuotas{.memoryBytes = size_t{8} * 1024 * 1024, .timePerCallMs = 50};

[[noreturn]] auto throwingBinding([[maybe_unused]] lua_State* iState) -> int { throw std::runtime_error("boom"); }

[[noreturn]] auto throwingIntBinding([[maybe_unused]] lua_State* iState) -> int { throw 42; }

/// Aborts the process when a test outlives its deadline, so a broken quota fails instead of freezing.
class Deadline final {
public:
	Deadline(const Deadline&) = delete;

	Deadline(Deadline&&) = delete;

	auto operator=(const Deadline&) -> Deadline& = delete;

	auto operator=(Deadline&&) -> Deadline& = delete;

	explicit Deadline(const std::chrono::seconds iDelay)
		: m_thread{[this, iDelay](const std::stop_token& iStop) -> void {
			  std::unique_lock<std::mutex> lock(m_mutex);
			  if (!m_wake.wait_for(lock, iStop, iDelay, []() -> bool { return false; }) && !iStop.stop_requested()) {
				  std::fputs(std::format("LuaSandbox: test exceeded its {} s deadline, a quota did not fire.\n",
										 iDelay.count())
									 .c_str(),
							 stderr);
				  std::abort();
			  }
		  }} {}

	~Deadline() = default;

private:
	/// Guards the wait.
	std::mutex m_mutex;
	/// Wait primitive.
	std::condition_variable_any m_wake;
	/// Guard thread, stopped on destruction.
	std::jthread m_thread;
};

class LuaSandbox : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }

	/// Fail-safe against a quota that never fires.
	Deadline deadline{std::chrono::seconds(20)};

	/// Engine with small quotas so the hostile cases end fast.
	LuaEngine engine{g_SmallQuotas};

	[[nodiscard]] auto run(const std::string& iScript) const -> bool {
		return engine.loadBuffer(toBuffer(iScript), "sandbox_test");
	}

	[[nodiscard]] auto flag(const std::string& iName) const -> bool {
		return engine.getGlobalBool(iName).value_or(false);
	}
};

}// namespace

TEST_F(LuaSandbox, dangerousGlobalsAbsent) {
	ASSERT_TRUE(run("ok = io == nil and os == nil and debug == nil and require == nil and package == nil "
					"and dofile == nil and loadfile == nil and string.dump == nil"));
	EXPECT_TRUE(flag("ok"));
}

TEST_F(LuaSandbox, hostRejectsBytecode) {
	const std::string bytecode = "\x1bLua\x55\x00 fake binary chunk";
	EXPECT_FALSE(engine.loadBuffer({bytecode.begin(), bytecode.end()}, "bytecode"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::LoadError);
}

TEST_F(LuaSandbox, scriptLoadIsTextOnly) {
	ASSERT_TRUE(run("local f, err = load('\\27Lua fake')\n"
					"binaryRefused = f == nil and err:find('binary') ~= nil\n"
					"local g = load('\\27Lua fake', 'x', 'b')\n"
					"forcedModeRefused = g == nil\n"
					"textValue = load('return 2 + 3')()\n"
					"local env = {}\n"
					"load('y = 7', 'envchunk', 't', env)()\n"
					"envRespected = env.y == 7 and y == nil\n"));
	EXPECT_TRUE(flag("binaryRefused"));
	EXPECT_TRUE(flag("forcedModeRefused"));
	EXPECT_EQ(engine.getGlobalInt("textValue"), 5);
	EXPECT_TRUE(flag("envRespected"));
}

TEST_F(LuaSandbox, metatablesRestricted) {
	ASSERT_TRUE(run("local ok = pcall(setmetatable, {}, {__gc = function() end})\n"
					"gcRefused = not ok\n"
					"plain = getmetatable(setmetatable({}, {__index = {a = 1}})) ~= nil\n"
					"stringLocked = getmetatable('') == false\n"
					"stringMethods = ('abc'):upper() == 'ABC'\n"));
	EXPECT_TRUE(flag("gcRefused"));
	EXPECT_TRUE(flag("plain"));
	EXPECT_TRUE(flag("stringLocked"));
	EXPECT_TRUE(flag("stringMethods"));
}

TEST_F(LuaSandbox, collectGarbageRestricted) {
	ASSERT_TRUE(run("countWorks = type(collectgarbage('count')) == 'number'\n"
					"collectRefused = not pcall(collectgarbage)\n"
					"stopRefused = not pcall(collectgarbage, 'stop')\n"));
	EXPECT_TRUE(flag("countWorks"));
	EXPECT_TRUE(flag("collectRefused"));
	EXPECT_TRUE(flag("stopRefused"));
}

TEST_F(LuaSandbox, infiniteLoopStopped) {
	ASSERT_TRUE(run("function spin() while true do end end\n"
					"function catcher() while true do pcall(function() while true do end end) end end\n"
					"function coro() while true do coroutine.resume(coroutine.create(function() while true do end "
					"end)) end end\n"
					"function wrapped() coroutine.wrap(function() while true do end end)() end\n"
					"function nested() local inner = coroutine.create(function() while true do end end)\n"
					"  coroutine.resume(coroutine.create(function() while true do coroutine.resume(inner) end end))\n"
					"  while true do end end\n"
					"function fine() local s = 0 for i = 1, 1000 do s = s + i end result = s end\n"));
	const auto start = std::chrono::steady_clock::now();
	for (const auto* name: {"spin", "catcher", "coro", "wrapped", "nested"}) {
		EXPECT_FALSE(engine.callFunction(name)) << name;
		EXPECT_EQ(engine.getLastStatus(), LuaStatus::TimeQuota) << name;
	}
	EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));
	// The budget is per call: the state stays usable afterwards.
	EXPECT_TRUE(engine.callFunction("fine"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::Ok);
	EXPECT_EQ(engine.getGlobalInt("result"), 500500);
}

TEST_F(LuaSandbox, coroutineWrapStillWorks) {
	ASSERT_TRUE(run("local g = coroutine.wrap(function(a) local b = coroutine.yield(a + 1) return b * 2 end)\n"
					"wrapA = g(1)\nwrapB = g(5)\n"
					"wrapError = not pcall(coroutine.wrap(function() error('inside') end))\n"));
	EXPECT_EQ(engine.getGlobalInt("wrapA"), 2);
	EXPECT_EQ(engine.getGlobalInt("wrapB"), 10);
	EXPECT_TRUE(flag("wrapError"));
}

TEST_F(LuaSandbox, topLevelLoopStopped) {
	EXPECT_FALSE(run("while true do end"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::TimeQuota);
}

TEST_F(LuaSandbox, budgetResetBetweenCalls) {
	ASSERT_TRUE(run("function work() local s = 0 for i = 1, 20000 do s = s + i end end"));
	for (int i = 0; i < 20; ++i) EXPECT_TRUE(engine.callFunction("work"));
}

TEST_F(LuaSandbox, massiveAllocationRefused) {
	engine.setQuotas({.memoryBytes = size_t{8} * 1024 * 1024, .timePerCallMs = 0});
	ASSERT_TRUE(run("function huge() big = string.rep('x', 1 << 30) end\n"
					"function grow() local t = {} local i = 1 while true do t[i] = {i} i = i + 1 end end\n"
					"function caught() local ok = pcall(string.rep, 'x', 1 << 30) survived = not ok end\n"));
	EXPECT_FALSE(engine.callFunction("huge"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::MemoryQuota);
	EXPECT_FALSE(engine.callFunction("grow"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::MemoryQuota);
	EXPECT_LE(engine.getMemoryUsage(), size_t{8} * 1024 * 1024);
	ASSERT_TRUE(run("t = nil"));
	EXPECT_TRUE(engine.callFunction("caught"));
	EXPECT_TRUE(flag("survived"));
}

TEST_F(LuaSandbox, memoryCeilingRestoredAfterCaughtBindingError) {
	engine.setQuotas({.memoryBytes = size_t{8} * 1024 * 1024, .timePerCallMs = 0});
	registerBindings(engine.getState());
	ASSERT_TRUE(run("function sneaky() pcall(log.info, {}) big = string.rep('x', 1 << 30) end"));
	EXPECT_FALSE(engine.callFunction("sneaky"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::MemoryQuota);
}

TEST_F(LuaSandbox, errorInCallbackIsRuntimeError) {
	ASSERT_TRUE(run("calls = 0\nfunction on_update(dt) calls = calls + 1 error('scripted failure') end"));
	EXPECT_FALSE(engine.callFunction("on_update", 0.016f));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::RuntimeError);
	EXPECT_FALSE(engine.callFunction("on_update", 0.016f));
	EXPECT_EQ(engine.getGlobalInt("calls"), 2);
	ASSERT_TRUE(run("function tableError() error({}) end"));
	EXPECT_FALSE(engine.callFunction("tableError"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::RuntimeError);
}

TEST_F(LuaSandbox, cppExceptionInBindingBecomesLuaError) {
	static constexpr luaL_Reg funcs[] = {{"throw", throwingBinding},
										 {"throw_int", throwingIntBinding},
										 {nullptr, nullptr}};
	LuaEngine::registerGuardedTable(engine.getState(), "hostile", funcs);
	ASSERT_TRUE(run("local ok, err = pcall(hostile.throw)\n"
					"caught = not ok and err:find('C%+%+ exception: boom') ~= nil\n"
					"local ok2, err2 = pcall(hostile.throw_int)\n"
					"caughtUnknown = not ok2 and err2:find('unknown C%+%+ exception') ~= nil\n"
					"function on_update() hostile.throw() end\n"));
	EXPECT_TRUE(flag("caught"));
	EXPECT_TRUE(flag("caughtUnknown"));
	EXPECT_FALSE(engine.callFunction("on_update"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::RuntimeError);
}

TEST_F(LuaSandbox, bindingArgumentErrorNamesTheBinding) {
	registerBindings(engine.getState());
	ASSERT_TRUE(run("local ok, err = pcall(function() log.info({}) end)\n"
					"named = not ok and err:find(\"'info'\", 1, true) ~= nil\n"));
	EXPECT_TRUE(flag("named"));
}

TEST_F(LuaSandbox, hostileGlobalMetatableIgnoredByHost) {
	ASSERT_TRUE(run("speed = 3.5\n"
					"setmetatable(_G, {__index = function() while true do end end,\n"
					"                  __newindex = function() error('blocked') end})\n"));
	EXPECT_FALSE(engine.hasFunction("on_update"));
	EXPECT_FALSE(engine.callFunction("on_update"));
	EXPECT_EQ(engine.getLastStatus(), LuaStatus::Missing);
	EXPECT_FALSE(engine.getGlobalFloat("missing").has_value());
	engine.setGlobal("speed", 9.0f);
	EXPECT_FLOAT_EQ(engine.getGlobalFloat("speed").value_or(0.f), 9.0f);
}

TEST_F(LuaSandbox, extractPropertiesSurvivesHostileScript) {
	const auto loop = ScriptEngine::extractPropertiesFromBuffer(toBuffer("while true do end"), "loop");
	EXPECT_TRUE(loop.empty());
	const auto props = ScriptEngine::extractPropertiesFromBuffer(
			toBuffer("properties = setmetatable({ {name = 'speed', type = 'float', default = 2.0} },\n"
					 "  {__index = function() while true do end end})\n"),
			"metatable");
	ASSERT_EQ(props.size(), 1);
	EXPECT_EQ(props.front().name, "speed");
}

TEST_F(LuaSandbox, defaultQuotas) {
	const auto saved = ScriptEngine::getDefaultQuotas();
	EXPECT_EQ(saved.memoryBytes, size_t{64} * 1024 * 1024);
	EXPECT_EQ(saved.timePerCallMs, 250u);
	ScriptEngine::setDefaultQuotas(g_SmallQuotas);
	const LuaEngine fresh;
	EXPECT_EQ(fresh.getQuotas().memoryBytes, g_SmallQuotas.memoryBytes);
	EXPECT_EQ(fresh.getQuotas().timePerCallMs, g_SmallQuotas.timePerCallMs);
	EXPECT_GT(fresh.getMemoryUsage(), 0u);
	ScriptEngine::setDefaultQuotas(saved);
}

TEST_F(LuaSandbox, instanceDisabledOnQuota) {
	const ScriptInstance inst;
	inst.setQuotas(g_SmallQuotas);
	EXPECT_EQ(inst.getQuotas().timePerCallMs, g_SmallQuotas.timePerCallMs);
	ASSERT_TRUE(inst.createFromBuffer(toBuffer("calls = 0\n"
											   "function on_update(dt) calls = calls + 1 while true do end end\n"),
									  "runaway", 1));
	inst.onUpdate(0.016f);
	EXPECT_TRUE(inst.isDisabled());
	inst.onUpdate(0.016f);
	EXPECT_EQ(inst.getPropertyInt("calls"), 1);
	EXPECT_FALSE(inst.callFunction("on_update"));
}

TEST_F(LuaSandbox, instanceKeepsRunningAfterRuntimeError) {
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(toBuffer("calls = 0\n"
											   "function on_update(dt) calls = calls + 1 error('oops') end\n"),
									  "flaky", 1));
	inst.onUpdate(0.016f);
	inst.onUpdate(0.016f);
	EXPECT_FALSE(inst.isDisabled());
	EXPECT_EQ(inst.getPropertyInt("calls"), 2);
}

TEST_F(LuaSandbox, instanceDisabledOnMemoryQuota) {
	const ScriptInstance inst;
	inst.setQuotas({.memoryBytes = size_t{4} * 1024 * 1024, .timePerCallMs = 0});
	ASSERT_TRUE(inst.createFromBuffer(toBuffer("function on_update(dt) hoard = string.rep('x', 1 << 28) end\n"),
									  "hoarder", 1));
	inst.onUpdate(0.016f);
	EXPECT_TRUE(inst.isDisabled());
}
