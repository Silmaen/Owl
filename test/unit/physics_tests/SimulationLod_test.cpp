/**
 * @file SimulationLod_test.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <physics/PhysicCommand.h>
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
using namespace owl::physics;
using namespace owl::scene;

namespace {

auto makeFrame() -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(16'667));
	return ts;
}

auto counter(Scene& ioScene, const std::string& iKey) -> int64_t {
	const auto value = ioScene.getGameState().get(iKey);
	return value && std::holds_alternative<int64_t>(*value) ? std::get<int64_t>(*value) : 0;
}

class SimulationLodTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		std::filesystem::create_directories(m_dir);
		std::ofstream{m_dir / "far.lua"} << R"(
local function bump(key) gamestate.set(key, (gamestate.get(key) or 0) + 1) end
function on_tick(dt) bump('ticks') end
function on_demoted() bump('demoted') end
function on_promoted() bump('promoted') end
function on_coarse_tick(dt) bump('coarse'); gamestate.set('coarse_dt', dt) end
)";
		m_scene.getPhysicsSettings().gravity = {0.f, 0.f, 0.f};
		m_camera = m_scene.createEntity("camera");
		m_camera.addComponent<component::Camera>().primary = true;
		m_far = m_scene.createEntity("far");
		m_far.getComponent<component::Transform>().transform.translation() = {500.f, 0.f, 0.f};
		auto& lod = m_far.addComponent<component::SimulationLod>();
		lod.radius = 100.f;
		lod.hysteresis = 10.f;
		lod.coarseInterval = 5;
		m_far.addComponent<component::LuaScript>().scriptPath = (m_dir / "far.lua").generic_string();
		auto& [body] = m_far.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
		body.dimension = SceneBody::Dimension::Space;
		body.shape = SceneBody::Shape::Sphere;
	}

	void TearDown() override {
		m_scene.onEndRuntime();
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	void run(const int iFrames) {
		for (int frame = 0; frame < iFrames; ++frame) m_scene.onUpdateRuntime(makeFrame(), false);
	}

	std::filesystem::path m_dir = std::filesystem::temp_directory_path() / "owl_simulation_lod";
	Scene m_scene;
	Entity m_camera;
	Entity m_far;
};

}// namespace

TEST_F(SimulationLodTest, AFarEntityRunsCoarseTicksAndComesBackNear) {
	m_scene.onStartRuntime();
	PhysicCommand::setVelocity3D(m_far, {2.f, 0.f, 0.f});
	run(31);
	const auto& lod = m_far.getComponent<component::SimulationLod>();
	EXPECT_TRUE(lod.m_demoted);
	EXPECT_EQ(counter(m_scene, "demoted"), 1);
	EXPECT_EQ(counter(m_scene, "ticks"), 0);
	EXPECT_GE(counter(m_scene, "coarse"), 5);
	const auto dt = m_scene.getGameState().get("coarse_dt");
	ASSERT_TRUE(dt.has_value());
	EXPECT_NEAR(std::get<float>(*dt), 5.f / 60.f, 1e-4f);
	EXPECT_FALSE(PhysicCommand::isSimulated(m_far));
	// The camera comes near: the entity is promoted and its body wakes with the velocity it had.
	m_camera.getComponent<component::Transform>().transform.translation() = {450.f, 0.f, 0.f};
	run(2);
	EXPECT_FALSE(lod.m_demoted);
	EXPECT_EQ(counter(m_scene, "promoted"), 1);
	EXPECT_TRUE(PhysicCommand::isSimulated(m_far));
	EXPECT_GT(counter(m_scene, "ticks"), 0);
	EXPECT_NEAR(PhysicCommand::getVelocity3D(m_far).x(), 2.f, 1e-4f);
}

TEST_F(SimulationLodTest, OnRailsADemotedBodyKeepsMoving) {
	m_scene.onStartRuntime();
	PhysicCommand::setVelocity3D(m_far, {3.f, 0.f, 0.f});
	run(2);
	const float start = m_far.getComponent<component::Transform>().transform.translation().x();
	run(60);
	const float moved = m_far.getComponent<component::Transform>().transform.translation().x() - start;
	EXPECT_NEAR(moved, 3.f, 0.3f);
}

TEST_F(SimulationLodTest, AHeldBodyStaysWhereItWasDemoted) {
	m_far.getComponent<component::SimulationLod>().model = component::SimulationLod::Model::Hold;
	m_scene.onStartRuntime();
	PhysicCommand::setVelocity3D(m_far, {3.f, 0.f, 0.f});
	run(2);
	const float start = m_far.getComponent<component::Transform>().transform.translation().x();
	run(60);
	EXPECT_FLOAT_EQ(m_far.getComponent<component::Transform>().transform.translation().x(), start);
}

TEST_F(SimulationLodTest, HysteresisKeepsTheEntityOnItsSide) {
	m_far.getComponent<component::Transform>().transform.translation() = {105.f, 0.f, 0.f};
	m_scene.onStartRuntime();
	run(3);
	EXPECT_FALSE(m_far.getComponent<component::SimulationLod>().m_demoted);
	m_far.getComponent<component::SimulationLod>().enabled = false;
	PhysicCommand::setTransform3D(m_far, {1000.f, 0.f, 0.f}, {0.f, 0.f, 0.f});
	run(3);
	EXPECT_FALSE(m_far.getComponent<component::SimulationLod>().m_demoted);
}
