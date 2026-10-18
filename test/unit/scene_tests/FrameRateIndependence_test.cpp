/**
 * @file FrameRateIndependence_test.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::scene;

namespace {

class FrameRateIndependenceTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_frame_rate_independence_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		OWL_REQUIRE_MODULE(PHYSICS);
		OWL_REQUIRE_MODULE(SCRIPT);
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] auto writeScript(const std::string& iName, const std::string& iCode) const -> std::string {
		const auto path = m_dir / iName;
		std::ofstream file(path);
		file << iCode;
		return path.string();
	}

	std::filesystem::path m_dir;
};

// Moves the transform by speed * dt: the distance only depends on the summed delta times.
constexpr auto g_dtMover = R"(
function on_update(dt)
    local x, y, z = transform.get_position(entity_id)
    transform.set_position(entity_id, x + 2.0 * dt, y, z)
end
)";

// Top-down player of the sample world map: a velocity set every frame.
constexpr auto g_velocityMover = R"(
function on_create()
    physics.set_gravity_scale(entity_id, 0)
end
function on_update(dt)
    physics.set_velocity(entity_id, 2.0, 0.0)
end
)";

// Raycast player of the sample: turning re-applies the rendered pose to the body every frame.
constexpr auto g_turningMover = R"(
function on_create()
    physics.set_gravity_scale(entity_id, 0)
end
function on_update(dt)
    local px, py, _ = transform.get_position(entity_id)
    local _, _, rz = transform.get_rotation(entity_id)
    physics.set_transform(entity_id, px, py, rz + 0.5 * dt)
    physics.set_velocity(entity_id, 2.0, 0.0)
end
)";

constexpr int64_t g_runMicroseconds = 2'000'000;

// Two seconds of Play at a constant frame rate; returns the entity's final x position.
auto simulate(const std::string& iScript, const bool iPhysics, const int64_t iFrameMicroseconds) -> float {
	Scene scene;
	auto player = scene.createEntity("Player");
	if (iPhysics)
		player.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
	player.addComponent<component::LuaScript>().scriptPath = iScript;
	scene.onStartRuntime();
	core::Timestep frame;
	for (int64_t elapsed = 0; elapsed < g_runMicroseconds; elapsed += iFrameMicroseconds) {
		frame.forceUpdate(std::chrono::microseconds(iFrameMicroseconds));
		scene.onUpdateRuntime(frame, false);
	}
	const float x = player.getComponent<component::Transform>().transform.translation().x();
	scene.onEndRuntime();
	return x;
}

}// namespace

TEST(TimestepPrecision, SecondsKeepSubMillisecondFrames) {
	core::Timestep frame;
	frame.forceUpdate(std::chrono::microseconds(3'333));
	EXPECT_NEAR(frame.getSeconds(), 0.003333f, 1e-6f);
	frame.forceUpdate(std::chrono::microseconds(500));
	EXPECT_NEAR(frame.getSeconds(), 0.0005f, 1e-7f);
	EXPECT_NEAR(frame.getMilliseconds(), 0.5f, 1e-4f);
}

TEST_F(FrameRateIndependenceTest, ScriptDeltaTimeDoesNotDependOnFrameRate) {
	const auto script = writeScript("dt_mover.lua", g_dtMover);
	for (const int64_t frameUs: {16'667, 3'333, 1'000, 500}) {
		const float x = simulate(script, false, frameUs);
		EXPECT_NEAR(x, 4.f, 0.05f) << "frame of " << frameUs << " us";
	}
}

TEST_F(FrameRateIndependenceTest, VelocityDrivenBodyDoesNotDependOnFrameRate) {
	const auto script = writeScript("velocity_mover.lua", g_velocityMover);
	const float reference = simulate(script, true, 16'667);
	EXPECT_NEAR(reference, 4.f, 0.1f);
	for (const int64_t frameUs: {3'333, 1'000, 500}) {
		const float x = simulate(script, true, frameUs);
		EXPECT_NEAR(x, reference, 0.1f) << "frame of " << frameUs << " us";
	}
}

TEST_F(FrameRateIndependenceTest, BodyTeleportedToItsOwnPoseDoesNotDependOnFrameRate) {
	const auto script = writeScript("turning_mover.lua", g_turningMover);
	const float reference = simulate(script, true, 16'667);
	EXPECT_NEAR(reference, 4.f, 0.1f);
	for (const int64_t frameUs: {3'333, 1'000, 500}) {
		const float x = simulate(script, true, frameUs);
		EXPECT_NEAR(x, reference, 0.1f) << "frame of " << frameUs << " us";
	}
}
