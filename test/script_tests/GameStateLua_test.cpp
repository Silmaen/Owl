/**
 * @file GameStateLua_test.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Scene.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>

using namespace owl;
using namespace owl::script;

TEST(GameStateLua, setAndGetInt) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "function on_create()\n"
							   "  gamestate.set('coins', 42)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_int", 1));
	inst.onCreate();

	const auto val = scn->getGameState().get("coins");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(std::get<int64_t>(val.value()), 42);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(GameStateLua, setAndGetFloat) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "function on_create()\n"
							   "  gamestate.set('speed', 3.14)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_float", 1));
	inst.onCreate();

	const auto val = scn->getGameState().get("speed");
	ASSERT_TRUE(val.has_value());
	EXPECT_NEAR(std::get<float>(val.value()), 3.14f, 0.01f);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(GameStateLua, setAndGetString) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "function on_create()\n"
							   "  gamestate.set('name', 'hero')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_string", 1));
	inst.onCreate();

	const auto val = scn->getGameState().get("name");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(std::get<std::string>(val.value()), "hero");

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(GameStateLua, setAndGetBool) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "function on_create()\n"
							   "  gamestate.set('unlocked', true)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_bool", 1));
	inst.onCreate();

	const auto val = scn->getGameState().get("unlocked");
	ASSERT_TRUE(val.has_value());
	EXPECT_TRUE(std::get<bool>(val.value()));

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(GameStateLua, getWithDefault) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "result = 0\n"
							   "function on_create()\n"
							   "  result = gamestate.get('missing', 99)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_default", 1));
	inst.onCreate();

	const auto val = inst.getPropertyInt("result");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(val.value(), 99);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(GameStateLua, getReturnsStoredValue) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	scn->getGameState().set("level", int64_t{5});
	ScriptEngine::init(scn.get());

	const std::string script = "result = 0\n"
							   "function on_create()\n"
							   "  result = gamestate.get('level')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_read", 1));
	inst.onCreate();

	const auto val = inst.getPropertyInt("result");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(val.value(), 5);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(GameStateLua, removeAndClear) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "function on_create()\n"
							   "  gamestate.set('a', 1)\n"
							   "  gamestate.set('b', 2)\n"
							   "  gamestate.remove('a')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_remove", 1));
	inst.onCreate();

	EXPECT_FALSE(scn->getGameState().get("a").has_value());
	EXPECT_TRUE(scn->getGameState().get("b").has_value());

	ScriptEngine::shutdown();
	core::Log::invalidate();
}
