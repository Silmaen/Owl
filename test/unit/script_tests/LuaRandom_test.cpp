/**
 * @file LuaRandom_test.cpp
 * @author Silmaen
 * @date 03/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <math/elementaryFunctions.h>
#include <scene/Entity.h>
#include <scene/GameState.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <variant>

using namespace owl;
using namespace owl::scene;

namespace {

/// Draws from `math.random` in `on_create` and keeps the values in the game state.
constexpr auto g_Script = R"(
function on_create()
	gamestate.set('unit', math.random())
	gamestate.set('dice', math.random(6))
	gamestate.set('range', math.random(-3, 3))
	local keys = {}
	for key in pairs({alpha = 1, beta = 2, gamma = 3, delta = 4}) do keys[#keys + 1] = key end
	gamestate.set('order', table.concat(keys, ','))
	math.randomseed(5)
	gamestate.set('reseeded', math.random(1000000))
	gamestate.set('empty', pcall(math.random, 3, 1))
	gamestate.set('sin', math.sin(0.7))
	gamestate.set('atan', math.atan(1, -2))
	gamestate.set('log', math.log(8, 2))
	gamestate.set('pow', math.pow(2, 10))
	local mixed = {}
	mixed.z = 1
	mixed[3] = 1
	mixed.b = 1
	mixed[true] = 1
	mixed[1.5] = 1
	mixed[false] = 1
	local parts = {}
	for _, key in ipairs(table.sorted_keys(mixed)) do parts[#parts + 1] = tostring(key) end
	gamestate.set('sorted', table.concat(parts, ','))
	gamestate.set('unsortable', pcall(table.sorted_keys, {[{}] = 1}))
end
)";

class LuaRandom : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_lua_random_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir);
		std::ofstream{m_dir / "random.lua"} << g_Script;
	}

	void TearDown() override {
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	/// Run the script once in a Play session of a scene of the given seed.
	[[nodiscard]] auto play(const uint64_t iSeed) const -> GameState {
		const auto scene = mkShared<Scene>();
		scene->setRandomSeed(iSeed);
		auto entity = scene->createEntityWithUUID(core::UUID{1}, "Roller");
		entity.addComponent<component::LuaScript>().scriptPath = (m_dir / "random.lua").generic_string();
		scene->onStartRuntime();
		auto state = scene->getGameState();
		scene->onEndRuntime();
		return state;
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(LuaRandom, SameSeedGivesSameValuesAndTableOrder) {
	const auto first = play(42);
	const auto second = play(42);
	for (const auto* key: {"unit", "dice", "range", "order", "reseeded"}) {
		ASSERT_TRUE(first.get(key).has_value()) << key;
		EXPECT_EQ(first.get(key), second.get(key)) << key;
	}
	const auto unit = std::get<float>(*first.get("unit"));
	EXPECT_GE(unit, 0.f);
	EXPECT_LT(unit, 1.f);
	const auto dice = std::get<int64_t>(*first.get("dice"));
	EXPECT_GE(dice, 1);
	EXPECT_LE(dice, 6);
	EXPECT_EQ(first.get("empty"), (GameState::Value{false}));
}

TEST_F(LuaRandom, SeedChangesTheValuesButNotTheReseededOnes) {
	const auto first = play(42);
	const auto other = play(43);
	EXPECT_NE(first.get("unit"), other.get("unit"));
	EXPECT_EQ(first.get("reseeded"), other.get("reseeded"));
}

TEST_F(LuaRandom, MathFunctionsUseTheEngineMaths) {
	const auto state = play(0);
	EXPECT_EQ(state.get("sin"), (GameState::Value{static_cast<float>(math::sin(0.7))}));
	EXPECT_EQ(state.get("atan"), (GameState::Value{static_cast<float>(math::atan2(1.0, -2.0))}));
	// A whole number stays a whole number in the game state: Luau has one number type.
	EXPECT_EQ(state.get("log"), (GameState::Value{int64_t{3}}));
	EXPECT_EQ(state.get("pow"), (GameState::Value{int64_t{1024}}));
}

TEST_F(LuaRandom, SortedKeysGiveAFixedOrder) {
	const auto state = play(0);
	EXPECT_EQ(state.get("sorted"), (GameState::Value{std::string{"false,true,1.5,3,b,z"}}));
	EXPECT_EQ(state.get("unsortable"), (GameState::Value{false}));
}
