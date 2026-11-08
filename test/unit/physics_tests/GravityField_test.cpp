/**
 * @file GravityField_test.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <math/Orbit.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/GameState.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <system_error>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

auto makeFrame(const int64_t iMicroseconds = 16'667) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(iMicroseconds));
	return ts;
}

void runFrames(Scene& ioScene, const int iFrames) {
	for (int f = 0; f < iFrames; ++f) PhysicCommand::frame(ioScene, makeFrame());
}

auto addSource(Scene& ioScene, const component::GravitySource::Type iType, const math::vec3f& iPosition,
			   const float iStrength, const float iRadius) -> Entity {
	auto entity = ioScene.createEntity("source");
	entity.getComponent<component::Transform>().transform.translation() = iPosition;
	auto& source = entity.addComponent<component::GravitySource>();
	source.type = iType;
	source.strength = iStrength;
	source.radius = iRadius;
	return entity;
}

auto addBall(Scene& ioScene, const math::vec3f& iPosition,
			 const SceneBody::Dimension iDimension = SceneBody::Dimension::Space) -> Entity {
	auto entity = ioScene.createEntity("ball");
	entity.getComponent<component::Transform>().transform.translation() = iPosition;
	auto& [body] = entity.addComponent<component::PhysicBody>();
	body.type = SceneBody::BodyType::Dynamic;
	body.dimension = iDimension;
	body.shape = SceneBody::Shape::Sphere;
	body.colliderSize = {0.5f, 0.5f, 0.5f};
	return entity;
}

auto worldPositionOf(const Entity& iEntity) -> math::vec3f {
	return iEntity.getComponent<component::Transform>().transform.translation();
}

// No world gravity, for the bodies in space.
void toSpace(Scene& ioScene) { ioScene.getPhysicsSettings().gravity = {0.f, 0.f, 0.f}; }

class GravityFieldTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }
};

}// namespace

TEST_F(GravityFieldTest, TheFieldOfASourceFollowsItsType) {
	component::GravitySource source;
	source.strength = 10.f;
	source.radius = 5.f;
	EXPECT_NEAR(source.getAcceleration({10.f, 0.f, 0.f}).x(), -2.5f, 1e-5f);
	EXPECT_NEAR(source.getAcceleration({0.f, 2.5f, 0.f}).y(), -5.f, 1e-5f);
	source.type = component::GravitySource::Type::Point;
	EXPECT_NEAR(source.getAcceleration({0.f, 2.5f, 0.f}).y(), -10.f, 1e-5f);
	EXPECT_NEAR(source.getGravitationalParameter(), 250.0, 1e-9);
	source.range = 8.f;
	EXPECT_FLOAT_EQ(source.getAcceleration({10.f, 0.f, 0.f}).norm(), 0.f);
	source.type = component::GravitySource::Type::Uniform;
	source.direction = {2.f, 0.f, 0.f};
	EXPECT_NEAR(source.getAcceleration({1.f, 1.f, 1.f}).x(), 10.f, 1e-5f);
	source.enabled = false;
	EXPECT_FLOAT_EQ(source.getAcceleration({1.f, 1.f, 1.f}).norm(), 0.f);
}

TEST_F(GravityFieldTest, GravityAtAPointSumsTheWorldAndTheFields) {
	Scene scene;
	addSource(scene, component::GravitySource::Type::Point, {10.f, 0.f, 0.f}, 4.f, 1.f);
	addSource(scene, component::GravitySource::Type::Point, {-10.f, 0.f, 0.f}, 4.f, 1.f);
	const auto balanced = PhysicCommand::getGravityAt(scene, {0.f, 0.f, 0.f});
	EXPECT_NEAR(balanced.x(), 0.f, 1e-6f);
	EXPECT_NEAR(balanced.y(), -9.81f, 1e-5f);
	const auto nearer = PhysicCommand::getGravityAt(scene, {5.f, 0.f, 0.f});
	EXPECT_GT(nearer.x(), 0.f);
}

TEST_F(GravityFieldTest, ABodyFallsTowardAPointSource) {
	Scene scene;
	toSpace(scene);
	addSource(scene, component::GravitySource::Type::Point, {0.f, 0.f, 0.f}, 10.f, 2.f);
	const auto ball = addBall(scene, {0.f, 10.f, 0.f});
	PhysicCommand::init(scene);
	runFrames(scene, 120);
	const auto position = worldPositionOf(ball);
	// 0.4 at 10 units: about 0.8 units in two seconds, more as it nears.
	EXPECT_LT(position.y(), 9.2f);
	EXPECT_GT(position.y(), 2.f);
	EXPECT_NEAR(position.x(), 0.f, 1e-5f);
	EXPECT_NEAR(position.z(), 0.f, 1e-5f);
}

TEST_F(GravityFieldTest, ACircularOrbitKeepsItsRadius) {
	Scene scene;
	toSpace(scene);
	addSource(scene, component::GravitySource::Type::Spherical, {0.f, 0.f, 0.f}, 10.f, 5.f);
	const auto ball = addBall(scene, {20.f, 0.f, 0.f});
	PhysicCommand::init(scene);
	PhysicCommand::setVelocity3D(ball, {0.f, std::sqrt(250.f / 20.f), 0.f});
	float lowest = 20.f;
	float highest = 20.f;
	for (int frame = 0; frame < 2200; ++frame) {
		runFrames(scene, 1);
		const float radius = worldPositionOf(ball).norm();
		lowest = std::min(lowest, radius);
		highest = std::max(highest, radius);
	}
	EXPECT_GT(lowest, 19.6f);
	EXPECT_LT(highest, 20.4f);
}

TEST_F(GravityFieldTest, ABodyOptsOutOfTheFieldsOrScalesThem) {
	Scene scene;
	toSpace(scene);
	addSource(scene, component::GravitySource::Type::Point, {0.f, 0.f, 0.f}, 10.f, 2.f);
	auto free = addBall(scene, {0.f, 10.f, 0.f});
	free.getComponent<component::PhysicBody>().body.gravityFields = false;
	auto light = addBall(scene, {10.f, 0.f, 0.f});
	light.getComponent<component::PhysicBody>().body.gravityScale = 0.f;
	auto heavy = addBall(scene, {0.f, -10.f, 0.f});
	heavy.getComponent<component::PhysicBody>().body.gravityScale = 2.f;
	const auto normal = addBall(scene, {-10.f, 0.f, 0.f});
	PhysicCommand::init(scene);
	runFrames(scene, 20);
	EXPECT_FLOAT_EQ(worldPositionOf(free).y(), 10.f);
	EXPECT_FLOAT_EQ(worldPositionOf(light).x(), 10.f);
	EXPECT_NEAR(worldPositionOf(heavy).y() + 10.f, (worldPositionOf(normal).x() + 10.f) * 2.f, 1e-3f);
}

TEST_F(GravityFieldTest, ASourceOutOfRangePullsNothing) {
	Scene scene;
	toSpace(scene);
	auto source = addSource(scene, component::GravitySource::Type::Point, {0.f, 0.f, 0.f}, 10.f, 2.f);
	source.getComponent<component::GravitySource>().range = 5.f;
	const auto ball = addBall(scene, {0.f, 10.f, 0.f});
	PhysicCommand::init(scene);
	runFrames(scene, 20);
	EXPECT_FLOAT_EQ(worldPositionOf(ball).y(), 10.f);
}

TEST_F(GravityFieldTest, A2DBodyStaysInItsPlane) {
	Scene scene;
	toSpace(scene);
	addSource(scene, component::GravitySource::Type::Point, {0.f, 0.f, 5.f}, 10.f, 2.f);
	const auto ball = addBall(scene, {6.f, 0.f, 0.f}, SceneBody::Dimension::Plane);
	PhysicCommand::init(scene);
	runFrames(scene, 20);
	EXPECT_LT(worldPositionOf(ball).x(), 6.f);
	EXPECT_FLOAT_EQ(worldPositionOf(ball).z(), 0.f);
}

TEST_F(GravityFieldTest, ThePredictionFollowsTheFlight) {
	Scene scene;
	toSpace(scene);
	scene.getPhysicsSettings().interpolate = false;
	addSource(scene, component::GravitySource::Type::Spherical, {0.f, 0.f, 0.f}, 10.f, 5.f);
	const auto ball = addBall(scene, {15.f, 0.f, 0.f});
	PhysicCommand::init(scene);
	PhysicCommand::setVelocity3D(ball, {0.f, 3.f, 1.f});
	const auto path = PhysicCommand::predictTrajectory(ball, 120);
	ASSERT_EQ(path.size(), 120u);
	for (uint32_t step = 0; step < 120; ++step) {
		PhysicCommand::step(scene);
		PhysicCommand::writeFrameTransforms(scene);
		const auto position = worldPositionOf(ball);
		ASSERT_NEAR(position.x(), path[step].x(), 1e-3f) << "step " << step;
		ASSERT_NEAR(position.y(), path[step].y(), 1e-3f) << "step " << step;
		ASSERT_NEAR(position.z(), path[step].z(), 1e-3f) << "step " << step;
	}
}

TEST_F(GravityFieldTest, ThePredictionWorksWithoutAWorld) {
	Scene scene;
	const auto ball = addBall(scene, {0.f, 10.f, 0.f});
	const auto path = PhysicCommand::predictTrajectory(ball, 60);
	ASSERT_EQ(path.size(), 60u);
	EXPECT_NEAR(path.back().y(), 10.f - 0.5f * 9.81f, 0.1f);
	EXPECT_TRUE(PhysicCommand::predictTrajectory(scene.createEntity("none"), 10).empty());
}

TEST_F(GravityFieldTest, TwoRunsGiveTheSameBits) {
	const auto run = []() -> math::vec3f {
		Scene scene;
		toSpace(scene);
		addSource(scene, component::GravitySource::Type::Spherical, {0.f, 0.f, 0.f}, 10.f, 5.f);
		addSource(scene, component::GravitySource::Type::Point, {30.f, 5.f, 0.f}, 3.f, 1.f);
		const auto ball = addBall(scene, {12.f, 3.f, 1.f});
		PhysicCommand::init(scene);
		PhysicCommand::setVelocity3D(ball, {0.5f, 3.f, -0.2f});
		runFrames(scene, 300);
		return worldPositionOf(ball);
	};
	const auto first = run();
	const auto second = run();
	EXPECT_EQ(first.x(), second.x());
	EXPECT_EQ(first.y(), second.y());
	EXPECT_EQ(first.z(), second.z());
}

TEST_F(GravityFieldTest, TheSettingsAndTheComponentRoundTrip) {
	const auto scene = mkShared<Scene>();
	scene->getPhysicsSettings().gravity = {0.f, 0.f, -3.f};
	scene->getPhysicsSettings().railsTimeScale = 12.f;
	auto source = addSource(*scene, component::GravitySource::Type::Uniform, {1.f, 2.f, 3.f}, 4.f, 6.f);
	source.getComponent<component::GravitySource>().direction = {1.f, 0.f, 0.f};
	source.getComponent<component::GravitySource>().range = 7.f;
	auto ball = addBall(*scene, {0.f, 0.f, 0.f});
	ball.getComponent<component::PhysicBody>().body.gravityScale = 0.5f;
	ball.getComponent<component::PhysicBody>().body.gravityFields = false;
	const auto text = SceneSerializer(scene).serializeToString();
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(
			SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>(text.begin(), text.end())).has_value());
	EXPECT_FLOAT_EQ(loaded->getPhysicsSettings().gravity.z(), -3.f);
	EXPECT_FLOAT_EQ(loaded->getPhysicsSettings().railsTimeScale, 12.f);
	const auto sources = loaded->registry.view<component::GravitySource>();
	ASSERT_EQ(sources.size(), 1u);
	const auto& field = sources.get<component::GravitySource>(sources.front());
	EXPECT_EQ(field.type, component::GravitySource::Type::Uniform);
	EXPECT_FLOAT_EQ(field.range, 7.f);
	EXPECT_FLOAT_EQ(field.direction.x(), 1.f);
	const auto bodies = loaded->registry.view<component::PhysicBody>();
	ASSERT_EQ(bodies.size(), 1u);
	EXPECT_FLOAT_EQ(bodies.get<component::PhysicBody>(bodies.front()).body.gravityScale, 0.5f);
	EXPECT_FALSE(bodies.get<component::PhysicBody>(bodies.front()).body.gravityFields);
}

TEST_F(GravityFieldTest, AWarpPutsTheBodiesOnTheirArcs) {
	Scene scene;
	toSpace(scene);
	addSource(scene, component::GravitySource::Type::Spherical, {0.f, 0.f, 0.f}, 10.f, 5.f);
	const auto ball = addBall(scene, {20.f, 0.f, 0.f});
	scene.onStartRuntime();
	const math::vec3f velocity{0.f, std::sqrt(250.f / 20.f) * 1.1f, 0.5f};
	PhysicCommand::setVelocity3D(ball, velocity);
	scene.setTimeScale(80.f);
	const auto ticksBefore = scene.getSimulationClock().getTotalTickCount();
	for (int frame = 0; frame < 20; ++frame) scene.onUpdateRuntime(makeFrame(), false);
	EXPECT_TRUE(PhysicCommand::isOnRails(scene));
	EXPECT_DOUBLE_EQ(scene.getRailsFactor(), 10.0);
	const auto ticks = scene.getSimulationClock().getTotalTickCount() - ticksBefore;
	const double seconds = static_cast<double>(ticks) * scene.getSimulationClock().getStepSeconds() * 10.0;
	const auto expected = math::propagateKepler(
			{.position = {20.0, 0.0, 0.0},
			 .velocity = {0.0, static_cast<double>(velocity.y()), static_cast<double>(velocity.z())}},
			250.0, seconds);
	PhysicCommand::syncSimulatedTransforms(scene);
	const auto position = worldPositionOf(ball);
	EXPECT_NEAR(position.x(), expected.position.x(), 0.05);
	EXPECT_NEAR(position.y(), expected.position.y(), 0.05);
	EXPECT_NEAR(position.z(), expected.position.z(), 0.05);
	// Back below the rails scale, the solver takes the body over where the arc left it.
	scene.setTimeScale(1.f);
	scene.onUpdateRuntime(makeFrame(), false);
	EXPECT_FALSE(PhysicCommand::isOnRails(scene));
	PhysicCommand::syncSimulatedTransforms(scene);
	EXPECT_NEAR(worldPositionOf(ball).norm(), static_cast<float>(expected.position.norm()), 0.1f);
	scene.onEndRuntime();
}

TEST_F(GravityFieldTest, ASleepingBodyStaysPutOnRails) {
	Scene scene;
	auto ground = scene.createEntity("ground");
	ground.getComponent<component::Transform>().transform.scale() = {20.f, 1.f, 20.f};
	auto& [groundBody] = ground.addComponent<component::PhysicBody>();
	groundBody.type = SceneBody::BodyType::Static;
	groundBody.dimension = SceneBody::Dimension::Space;
	const auto ball = addBall(scene, {0.f, 1.f, 0.f});
	scene.onStartRuntime();
	for (int frame = 0; frame < 240; ++frame) scene.onUpdateRuntime(makeFrame(), false);
	const auto rest = worldPositionOf(ball);
	ASSERT_NEAR(rest.y(), 0.75f, 0.05f);
	scene.setTimeScale(1000.f);
	for (int frame = 0; frame < 20; ++frame) scene.onUpdateRuntime(makeFrame(), false);
	EXPECT_TRUE(PhysicCommand::isOnRails(scene));
	EXPECT_NEAR(worldPositionOf(ball).y(), rest.y(), 1e-4f);
	scene.onEndRuntime();
}

TEST_F(GravityFieldTest, LuaReadsTheGravityAndThePath) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_gravity_field_lua";
	std::filesystem::create_directories(dir);
	std::ofstream{dir / "probe.lua"} << R"(
function on_tick(dt)
	local gx, gy = physics.get_gravity(5, 0)
	gamestate.set('gx', gx)
	gamestate.set('gy', gy)
	local _, _, gz = physics.get_gravity(0, 0, 3)
	gamestate.set('gz', gz)
	local path = physics.predict_trajectory(gamestate.get('ball'), 10)
	gamestate.set('points', #path)
	gamestate.set('last_z', path[#path].z)
	gamestate.set('rails', physics.is_on_rails())
	gamestate.set('demoted', entity.is_demoted(gamestate.get('ball')))
	gamestate.set('field_on', entity.is_gravity_source_enabled(gamestate.get('source')))
end
)";
	Scene scene;
	toSpace(scene);
	const auto source = addSource(scene, component::GravitySource::Type::Point, {10.f, 0.f, 0.f}, 4.f, 2.f);
	const auto ball = addBall(scene, {0.f, 0.f, 5.f});
	scene.createEntity("probe").addComponent<component::LuaScript>().scriptPath = (dir / "probe.lua").generic_string();
	scene.onStartRuntime();
	scene.getGameState().set("ball", static_cast<int64_t>(static_cast<uint64_t>(ball.getUUID())));
	scene.getGameState().set("source", static_cast<int64_t>(static_cast<uint64_t>(source.getUUID())));
	for (int i = 0; i < 3; ++i) scene.onUpdateRuntime(makeFrame(), false);
	const auto& state = scene.getGameState();
	const auto read = [&state](const char* iKey) -> float {
		const auto value = state.get(iKey);
		return value && std::holds_alternative<float>(*value) ? std::get<float>(*value) : -1000.f;
	};
	EXPECT_NEAR(read("gx"), 4.f * 4.f / 25.f, 1e-4f);
	EXPECT_NEAR(read("gy"), 0.f, 1e-5f);
	EXPECT_LT(read("gz"), 0.f);
	EXPECT_NEAR(read("last_z"), 5.f, 0.1f);
	const auto points = state.get("points");
	ASSERT_TRUE(points.has_value());
	EXPECT_EQ(std::get<int64_t>(*points), 10);
	EXPECT_FALSE(std::get<bool>(state.get("rails").value_or(true)));
	EXPECT_FALSE(std::get<bool>(state.get("demoted").value_or(true)));
	EXPECT_TRUE(std::get<bool>(state.get("field_on").value_or(false)));
	scene.onEndRuntime();
	std::error_code error;
	std::filesystem::remove_all(dir, error);
}
