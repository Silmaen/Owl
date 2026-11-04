/**
 * @file WorldHashGolden_test.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/GameState.h>
#include <scene/Scene.h>
#include <scene/WorldHash.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using namespace owl;
using namespace owl::scene;

namespace {

// Kicks the box it runs on with random impulses and the engine maths, on the fixed tick only.
constexpr auto g_Script = R"(
local ticks = 0
function on_tick(dt)
	ticks = ticks + 1
	if ticks % 12 == 0 then
		local angle = math.random() * 2 * math.pi
		local strength = math.pow(1.5, math.random(1, 4)) * math.exp(-ticks / 400)
		physics.impulse(entity_id, math.cos(angle) * strength, math.abs(math.sin(angle)) * strength)
	end
	gamestate.set('angle', math.atan(math.sin(ticks), math.cos(ticks)))
end
)";

class WorldHashGoldenTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		OWL_REQUIRE_MODULE(SCRIPT);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_world_hash_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir);
		std::ofstream{m_dir / "kick.lua"} << g_Script;
	}

	void TearDown() override {
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	// Ground, a stack of boxes and one scripted box, stepped for 4 s of simulation; the hash of the last tick.
	[[nodiscard]] auto run() const -> uint64_t {
		Scene scene;
		scene.setRandomSeed(2026);
		auto ground = scene.createEntityWithUUID(core::UUID{1}, "Ground");
		ground.getComponent<component::Transform>().transform.scale() = {40.f, 1.f, 1.f};
		ground.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
		for (uint64_t i = 0; i < 9; ++i) {
			const uint64_t row = i / 3;
			auto box = scene.createEntityWithUUID(core::UUID{10 + i}, "Box");
			box.getComponent<component::Transform>().transform.translation() = {
					static_cast<float>(i % 3) * 1.05f - 1.f, 1.5f + static_cast<float>(row) * 1.1f, 0.f};
			box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
		}
		auto kicker = scene.createEntityWithUUID(core::UUID{100}, "Kicker");
		kicker.getComponent<component::Transform>().transform.translation() = {-5.f, 1.f, 0.f};
		kicker.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
		kicker.addComponent<component::LuaScript>().scriptPath = (m_dir / "kick.lua").generic_string();
		scene.onStartRuntime();
		// The script is loaded: a path without the temporary folder, which differs between processes, enters the hash.
		kicker.getComponent<component::LuaScript>().scriptPath = "kick.lua";
		core::Timestep frame;
		frame.forceUpdate(std::chrono::microseconds(16'667));
		for (int i = 0; i < 240; ++i) scene.onUpdateRuntime(frame, false);
		EXPECT_TRUE(scene.getGameState().get("angle").has_value());
		EXPECT_NE(kicker.getComponent<component::Transform>().transform.translation().x(), -5.f);
		const uint64_t hash = computeWorldHash(scene);
		scene.onEndRuntime();
		return hash;
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(WorldHashGoldenTest, SameRunGivesTheSameHash) { EXPECT_EQ(run(), run()); }

// The value is the same on every target: a change of it on one target only is a determinism bug; a change everywhere
// follows a change of the simulation or of the scene format, and the value is updated with it.
TEST_F(WorldHashGoldenTest, MatchesTheReferenceOnEveryTarget) { EXPECT_EQ(run(), 0x516f2e74d35b8f10ull); }
