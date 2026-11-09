/**
 * @file Physic3D_test.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/GameState.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>
#include <script/EntityHandle.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

auto makeFrame(const int64_t iMicroseconds) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(iMicroseconds));
	return ts;
}

void runFrames(Scene& ioScene, const int iFrames) {
	for (int f = 0; f < iFrames; ++f) PhysicCommand::frame(ioScene, makeFrame(16'667));
}

auto addGround(Scene& ioScene, const SceneBody::Dimension iDimension) -> Entity {
	auto ground = ioScene.createEntity("ground");
	ground.getComponent<component::Transform>().transform.scale() = {20.f, 1.f, 20.f};
	auto& [body] = ground.addComponent<component::PhysicBody>();
	body.type = SceneBody::BodyType::Static;
	body.dimension = iDimension;
	return ground;
}

auto addBody(Scene& ioScene, const SceneBody::Dimension iDimension, const SceneBody::Shape iShape,
			 const math::vec3f& iPosition) -> Entity {
	auto entity = ioScene.createEntity("body");
	entity.getComponent<component::Transform>().transform.translation() = iPosition;
	auto& [body] = entity.addComponent<component::PhysicBody>();
	body.type = SceneBody::BodyType::Dynamic;
	body.dimension = iDimension;
	body.shape = iShape;
	return entity;
}

auto translationOf(const Entity& iEntity) -> math::vec3f {
	return iEntity.getComponent<component::Transform>().transform.translation();
}

class Physic3DTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }
};

}// namespace

TEST_F(Physic3DTest, ShapesComeToRestOnTheGroundAtTheirSize) {
	Scene scene;
	addGround(scene, SceneBody::Dimension::Space);
	const auto box = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Box, {-4.f, 3.f, 0.f});
	const auto sphere = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Sphere, {0.f, 3.f, 0.f});
	auto capsule = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Capsule, {4.f, 3.f, 0.f});
	auto& [capsuleBody] = capsule.getComponent<component::PhysicBody>();
	capsuleBody.colliderSize = {0.5f, 2.f, 0.5f};
	capsuleBody.fixedRotation = true;
	auto cylinder = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Cylinder, {8.f, 3.f, 0.f});
	cylinder.getComponent<component::PhysicBody>().body.colliderSize = {0.6f, 0.9f, 0.6f};
	PhysicCommand::init(scene);
	runFrames(scene, 180);
	// The ground top is at y = 0.5.
	EXPECT_NEAR(translationOf(box).y(), 1.f, 0.02f);
	EXPECT_NEAR(translationOf(sphere).y(), 1.f, 0.02f);
	EXPECT_NEAR(translationOf(capsule).y(), 1.5f, 0.02f);
	EXPECT_NEAR(translationOf(cylinder).y(), 0.95f, 0.02f);
	PhysicCommand::destroy(scene);
}

TEST_F(Physic3DTest, ASpaceBodyMovesAlongZAndAPlaneBodyDoesNot) {
	Scene scene;
	const auto space = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Box, {-3.f, 5.f, 0.f});
	const auto plane = addBody(scene, SceneBody::Dimension::Plane, SceneBody::Shape::Box, {3.f, 5.f, 0.f});
	PhysicCommand::init(scene);
	PhysicCommand::impulse3D(space, {0.f, 0.f, 2.f});
	PhysicCommand::impulse3D(plane, {0.f, 0.f, 2.f});
	runFrames(scene, 30);
	EXPECT_NEAR(PhysicCommand::getVelocity3D(space).z(), 2.f, 1e-3f);
	EXPECT_GT(translationOf(space).z(), 0.9f);
	EXPECT_FLOAT_EQ(PhysicCommand::getVelocity3D(plane).z(), 0.f);
	EXPECT_FLOAT_EQ(translationOf(plane).z(), 0.f);
	PhysicCommand::destroy(scene);
}

TEST_F(Physic3DTest, PlaneAndSpaceBodiesCollide) {
	Scene scene;
	const auto ground = addGround(scene, SceneBody::Dimension::Plane);
	const auto ball = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Sphere, {0.f, 3.f, 0.f});
	PhysicCommand::init(scene);
	std::vector<PhysicCommand::CollisionEvent> events;
	for (int f = 0; f < 120; ++f) {
		runFrames(scene, 1);
		for (const auto& event: PhysicCommand::takeCollisionEvents(scene))
			if (!event.ended)
				events.push_back(event);
	}
	EXPECT_NEAR(translationOf(ball).y(), 1.f, 0.02f);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_TRUE((events.front().entityA == ground.getUUID() && events.front().entityB == ball.getUUID()) ||
				(events.front().entityA == ball.getUUID() && events.front().entityB == ground.getUUID()));
	PhysicCommand::destroy(scene);
}

TEST_F(Physic3DTest, ADiscRestsOnTheGroundAtItsRadius) {
	Scene scene;
	addGround(scene, SceneBody::Dimension::Plane);
	const auto disc = addBody(scene, SceneBody::Dimension::Plane, SceneBody::Shape::Sphere, {0.f, 3.f, 0.f});
	PhysicCommand::init(scene);
	runFrames(scene, 180);
	EXPECT_NEAR(translationOf(disc).y(), 1.f, 0.02f);
	EXPECT_FLOAT_EQ(translationOf(disc).z(), 0.f);
	PhysicCommand::destroy(scene);
}

TEST_F(Physic3DTest, TransformAndVelocityRoundTrip) {
	Scene scene;
	auto body = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Box, {0.f, 0.f, 0.f});
	PhysicCommand::init(scene);
	const math::vec3f rotation{0.3f, -0.2f, 0.1f};
	PhysicCommand::setTransform3D(body, {1.f, 2.f, 3.f}, rotation);
	PhysicCommand::setVelocity3D(body, {0.5f, 0.f, -1.f});
	const auto velocity = PhysicCommand::getVelocity3D(body);
	EXPECT_NEAR(velocity.x(), 0.5f, 1e-4f);
	EXPECT_NEAR(velocity.z(), -1.f, 1e-4f);
	PhysicCommand::syncSimulatedTransforms(scene);
	const auto& transform = body.getComponent<component::Transform>().transform;
	EXPECT_NEAR(transform.translation().x(), 1.f, 1e-4f);
	EXPECT_NEAR(transform.translation().y(), 2.f, 1e-4f);
	EXPECT_NEAR(transform.translation().z(), 3.f, 1e-4f);
	for (size_t i = 0; i < 3; ++i) EXPECT_NEAR(transform.rotation()[i], rotation[i], 1e-4f) << i;
	// The 2D call keeps Z and the rotations around X and Y.
	PhysicCommand::setTransform(body, {4.f, 5.f}, 0.4f);
	PhysicCommand::setVelocity(body, {2.f, 0.f});
	EXPECT_NEAR(PhysicCommand::getVelocity3D(body).z(), -1.f, 1e-4f);
	PhysicCommand::syncSimulatedTransforms(scene);
	EXPECT_NEAR(transform.translation().x(), 4.f, 1e-4f);
	EXPECT_NEAR(transform.translation().z(), 3.f, 1e-4f);
	EXPECT_NEAR(transform.rotation().x(), 0.3f, 1e-4f);
	EXPECT_NEAR(transform.rotation().z(), 0.4f, 1e-4f);
	PhysicCommand::destroy(scene);
}

TEST_F(Physic3DTest, ATumblingBodyGivesTheSameBitsWhateverTheWorkerCount) {
	const auto run = [](const uint32_t iWorkers) -> std::vector<math::vec3f> {
		Scene scene;
		scene.getPhysicsSettings().workerCount = iWorkers;
		addGround(scene, SceneBody::Dimension::Space);
		constexpr std::array shapes{SceneBody::Shape::Sphere, SceneBody::Shape::Capsule, SceneBody::Shape::Box};
		std::vector<Entity> bodies;
		for (uint32_t i = 0; i < 40; ++i) {
			const uint32_t layer = i / 5;
			auto body = addBody(scene, SceneBody::Dimension::Space, shapes.at(i % 3),
								{static_cast<float>(i % 5) * 0.7f - 1.4f, 2.f + static_cast<float>(layer) * 1.2f,
								 static_cast<float>(i % 3) * 0.6f - 0.6f});
			body.getComponent<component::Transform>().transform.rotation() = {0.1f * static_cast<float>(i), 0.f, 0.3f};
			bodies.push_back(body);
		}
		PhysicCommand::init(scene);
		runFrames(scene, 120);
		PhysicCommand::syncSimulatedTransforms(scene);
		std::vector<math::vec3f> state;
		for (const auto& body: bodies) {
			state.push_back(translationOf(body));
			state.push_back(body.getComponent<component::Transform>().transform.rotation());
		}
		PhysicCommand::destroy(scene);
		return state;
	};
	const auto mono = run(1);
	EXPECT_EQ(run(1), mono);
	EXPECT_EQ(run(4), mono);
}

TEST_F(Physic3DTest, ChildOfARotatedParentIsWrittenInItsParentSpace) {
	Scene scene;
	auto parent = scene.createEntity("parent");
	parent.getComponent<component::Transform>().transform.translation() = {10.f, 0.f, 0.f};
	parent.getComponent<component::Transform>().transform.rotation() = {0.f, 0.5f, 0.f};
	auto child = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Box, {0.f, 0.f, 0.f});
	scene.setParent(child, parent);
	PhysicCommand::init(scene);
	PhysicCommand::setTransform3D(child, {10.f, 4.f, 0.f}, {0.f, 0.5f, 0.f});
	PhysicCommand::syncSimulatedTransforms(scene);
	const auto& local = child.getComponent<component::Transform>().transform;
	EXPECT_NEAR(local.translation().x(), 0.f, 1e-4f);
	EXPECT_NEAR(local.translation().y(), 4.f, 1e-4f);
	EXPECT_NEAR(local.translation().z(), 0.f, 1e-4f);
	EXPECT_NEAR(local.rotation().y(), 0.f, 1e-4f);
	PhysicCommand::destroy(scene);
}

TEST_F(Physic3DTest, DimensionAndShapeRoundTripAndDefaultsStayUnwritten) {
	const auto scene = mkShared<Scene>();
	auto flat = scene->createEntityWithUUID(core::UUID{1}, "flat");
	flat.addComponent<component::PhysicBody>();
	auto solid = scene->createEntityWithUUID(core::UUID{2}, "solid");
	auto& [body] = solid.addComponent<component::PhysicBody>();
	body.dimension = SceneBody::Dimension::Space;
	body.shape = SceneBody::Shape::Capsule;
	const std::string text = SceneSerializer(scene).serializeToString();
	EXPECT_EQ(text.find("dimension: Plane"), std::string::npos);
	EXPECT_EQ(text.find("shape: Box"), std::string::npos);
	EXPECT_NE(text.find("dimension: Space"), std::string::npos);
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>(text.begin(), text.end()), "mem"));
	const auto& [flatBody] = loaded->findEntityByUUID(core::UUID{1}).getComponent<component::PhysicBody>();
	EXPECT_EQ(flatBody.dimension, SceneBody::Dimension::Plane);
	EXPECT_EQ(flatBody.shape, SceneBody::Shape::Box);
	const auto& [solidBody] = loaded->findEntityByUUID(core::UUID{2}).getComponent<component::PhysicBody>();
	EXPECT_EQ(solidBody.dimension, SceneBody::Dimension::Space);
	EXPECT_EQ(solidBody.shape, SceneBody::Shape::Capsule);
}

// The Lua table is the same for both dimensions: a 3D body gets and takes a third velocity component.
TEST_F(Physic3DTest, LuaVelocityHasAZComponentForASpaceBody) {
	OWL_REQUIRE_MODULE(SCRIPT);
	const auto dir = std::filesystem::temp_directory_path() / "owl_physic3d_lua";
	std::filesystem::create_directories(dir);
	std::ofstream{dir / "probe.lua"} << R"(
local ticks = 0
function on_tick(dt)
	ticks = ticks + 1
	if ticks == 1 then physics.set_velocity(entity_id, 1, 0, 3) end
	local vx, vy, vz = physics.get_velocity(entity_id)
	gamestate.set('vz_' .. entity_id, vz or -100.0)
end
)";
	Scene scene;
	auto space = addBody(scene, SceneBody::Dimension::Space, SceneBody::Shape::Box, {0.f, 5.f, 0.f});
	space.getComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Kinematic;
	space.addComponent<component::LuaScript>().scriptPath = (dir / "probe.lua").generic_string();
	auto plane = addBody(scene, SceneBody::Dimension::Plane, SceneBody::Shape::Box, {4.f, 5.f, 0.f});
	plane.getComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Kinematic;
	plane.addComponent<component::LuaScript>().scriptPath = (dir / "probe.lua").generic_string();
	scene.onStartRuntime();
	for (int i = 0; i < 30; ++i) scene.onUpdateRuntime(makeFrame(16'667), false);
	const auto spaceVz =
			scene.getGameState().get(std::format("vz_{}", static_cast<int64_t>(script::getEntityHandle(scene, space))));
	const auto planeVz =
			scene.getGameState().get(std::format("vz_{}", static_cast<int64_t>(script::getEntityHandle(scene, plane))));
	scene.onEndRuntime();
	std::error_code error;
	std::filesystem::remove_all(dir, error);
	ASSERT_TRUE(spaceVz.has_value());
	ASSERT_TRUE(planeVz.has_value());
	// A whole number comes back as an integer: Luau has one number type.
	const auto number = [](const scene::GameState::Value& iValue) -> float {
		return std::holds_alternative<int64_t>(iValue) ? static_cast<float>(std::get<int64_t>(iValue))
													   : std::get<float>(iValue);
	};
	EXPECT_NEAR(number(*spaceVz), 3.f, 1e-3f);
	EXPECT_NEAR(number(*planeVz), -100.f, 1e-3f);
}
