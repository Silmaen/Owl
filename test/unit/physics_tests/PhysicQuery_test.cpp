/**
 * @file PhysicQuery_test.cpp
 * @author Silmaen
 * @date 05/11/2026
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

auto addStatic(Scene& ioScene, const std::string& iName, const math::vec3f& iPosition, const math::vec3f& iScale)
		-> Entity {
	auto entity = ioScene.createEntity(iName);
	auto& transform = entity.getComponent<component::Transform>().transform;
	transform.translation() = iPosition;
	transform.scale() = iScale;
	entity.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
	return entity;
}

class PhysicQueryTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_ground = addStatic(m_scene, "ground", {0.f, 0.f, 0.f}, {20.f, 1.f, 1.f});
		m_wall = addStatic(m_scene, "wall", {5.f, 2.f, 0.f}, {1.f, 3.f, 1.f});
		m_sensor = addStatic(m_scene, "sensor", {-3.f, 2.f, 0.f}, {1.f, 3.f, 1.f});
		m_sensor.getComponent<component::PhysicBody>().body.sensor = true;
		m_ball = m_scene.createEntity("ball");
		m_ball.getComponent<component::Transform>().transform.translation() = {0.f, 3.f, 4.f};
		auto& [ball] = m_ball.addComponent<component::PhysicBody>();
		ball.type = SceneBody::BodyType::Static;
		ball.dimension = SceneBody::Dimension::Space;
		ball.shape = SceneBody::Shape::Sphere;
		ball.layer = 3;
		PhysicCommand::init(m_scene);
		PhysicCommand::frame(m_scene, makeFrame());
	}

	void TearDown() override {
		PhysicCommand::destroy(m_scene);
		core::Log::invalidate();
	}

	Scene m_scene;
	Entity m_ground;
	Entity m_wall;
	Entity m_sensor;
	Entity m_ball;
};

}// namespace

TEST_F(PhysicQueryTest, ARayStopsOnTheFirstSolidBody) {
	const auto down = PhysicCommand::raycast(m_scene, {0.f, 5.f, 0.f}, {0.f, -2.f, 0.f}, 10.f);
	ASSERT_TRUE(down.has_value());
	EXPECT_EQ(down->entity, m_ground.getUUID());
	EXPECT_NEAR(down->point.y(), 0.5f, 1e-3f);
	EXPECT_NEAR(down->normal.y(), 1.f, 1e-3f);
	EXPECT_NEAR(down->distance, 4.5f, 1e-3f);
	const auto right = PhysicCommand::raycast(m_scene, {0.f, 2.f, 0.f}, {1.f, 0.f, 0.f}, 10.f);
	ASSERT_TRUE(right.has_value());
	EXPECT_EQ(right->entity, m_wall.getUUID());
	EXPECT_NEAR(right->normal.x(), -1.f, 1e-3f);
	const auto left = PhysicCommand::raycast(m_scene, {0.f, 2.f, 0.f}, {-1.f, 0.f, 0.f}, 10.f);
	EXPECT_FALSE(left.has_value());
	EXPECT_FALSE(PhysicCommand::raycast(m_scene, {0.f, 5.f, 0.f}, {0.f, -1.f, 0.f}, 2.f).has_value());
	EXPECT_FALSE(PhysicCommand::raycast(m_scene, {0.f, 5.f, 0.f}, {0.f, 0.f, 0.f}, 10.f).has_value());
}

TEST_F(PhysicQueryTest, MasksAndDepthSelectWhatARayMeets) {
	const auto sphere = PhysicCommand::raycast(m_scene, {0.f, 3.f, 10.f}, {0.f, 0.f, -1.f}, 20.f);
	ASSERT_TRUE(sphere.has_value());
	EXPECT_EQ(sphere->entity, m_ball.getUUID());
	EXPECT_NEAR(sphere->point.z(), 4.5f, 1e-3f);
	const auto past = PhysicCommand::raycast(m_scene, {0.f, 3.f, 10.f}, {0.f, 0.f, -1.f}, 20.f,
											 static_cast<uint8_t>(SceneBody::allLayers & ~(1u << 3u)));
	EXPECT_FALSE(past.has_value());
}

TEST_F(PhysicQueryTest, AShapeCastHitsWhatTheRayMisses) {
	EXPECT_FALSE(PhysicCommand::raycast(m_scene, {0.f, 4.f, 0.f}, {1.f, 0.f, 0.f}, 10.f).has_value());
	const PhysicCommand::QueryShape disc{.sphere = true, .halfExtents = {0.6f, 0.6f, 0.6f}};
	const auto hit = PhysicCommand::shapeCast(m_scene, disc, {0.f, 4.f, 0.f}, {1.f, 0.f, 0.f}, 10.f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, m_wall.getUUID());
	const PhysicCommand::QueryShape box{.sphere = false, .halfExtents = {0.5f, 0.5f, 0.5f}};
	const auto boxHit = PhysicCommand::shapeCast(m_scene, box, {0.f, 2.f, 0.f}, {1.f, 0.f, 0.f}, 10.f);
	ASSERT_TRUE(boxHit.has_value());
	EXPECT_NEAR(boxHit->distance, 4.f, 0.02f);
}

TEST_F(PhysicQueryTest, AnOverlapListsTheSolidBodiesOnce) {
	const PhysicCommand::QueryShape box{.sphere = false, .halfExtents = {6.f, 2.f, 0.5f}};
	const auto found = PhysicCommand::overlap(m_scene, box, {0.f, 1.f, 0.f});
	ASSERT_EQ(found.size(), 2u);
	EXPECT_TRUE(found[0] == m_ground.getUUID() || found[1] == m_ground.getUUID());
	EXPECT_TRUE(found[0] == m_wall.getUUID() || found[1] == m_wall.getUUID());
	const PhysicCommand::QueryShape small{.sphere = true, .halfExtents = {0.2f, 0.2f, 0.2f}};
	EXPECT_TRUE(PhysicCommand::overlap(m_scene, small, {0.f, 3.f, 0.f}).empty());
	EXPECT_EQ(PhysicCommand::overlap(m_scene, small, {0.f, 3.f, 4.f}).size(), 1u);
}

TEST(PhysicQuery, WithoutAWorldNothingIsFound) {
	core::Log::init(core::Log::Level::Off);
	const Scene scene;
	EXPECT_FALSE(PhysicCommand::raycast(scene, {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, 1.f).has_value());
	EXPECT_TRUE(PhysicCommand::overlap(scene, {}, {0.f, 0.f, 0.f}).empty());
	core::Log::invalidate();
}

TEST(PhysicQuery, LuaRaycastReturnsTheHit) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_physic_query_lua";
	std::filesystem::create_directories(dir);
	std::ofstream{dir / "probe.lua"} << R"(
function on_tick(dt)
	local hit = physics.raycast(0, 5, 0, -1, 10)
	if hit then gamestate.set('hit_y', hit.y) end
	gamestate.set('miss', physics.raycast(0, 5, 0, 1, 10) == nil)
	gamestate.set('found', #physics.overlap_circle(0, 0, 1))
end
)";
	Scene scene;
	addStatic(scene, "ground", {0.f, 0.f, 0.f}, {20.f, 1.f, 1.f});
	scene.createEntity("probe").addComponent<component::LuaScript>().scriptPath = (dir / "probe.lua").generic_string();
	scene.onStartRuntime();
	for (int i = 0; i < 3; ++i) scene.onUpdateRuntime(makeFrame(), false);
	const auto hitY = scene.getGameState().get("hit_y");
	const auto miss = scene.getGameState().get("miss");
	const auto found = scene.getGameState().get("found");
	scene.onEndRuntime();
	std::error_code error;
	std::filesystem::remove_all(dir, error);
	ASSERT_TRUE(hitY.has_value());
	EXPECT_NEAR(std::get<float>(*hitY), 0.5f, 1e-3f);
	ASSERT_TRUE(miss.has_value());
	EXPECT_TRUE(std::get<bool>(*miss));
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(std::get<int64_t>(*found), 1);
	core::Log::invalidate();
}
