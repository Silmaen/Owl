/**
 * @file WorldPosition_test.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class WorldPositionTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(10));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto translationOf(const Entity& iEntity) -> math::vec3& {
		return iEntity.getComponent<component::Transform>().transform.translation();
	}
};

}// namespace

TEST_F(WorldPositionTest, AddingItKeepsTheEntityWhereItIs) {
	Scene scene;
	auto entity = scene.createEntity("Ship");
	translationOf(entity) = {3.f, -2.f, 7.f};
	const auto& world = entity.addComponent<component::WorldPosition>();
	EXPECT_DOUBLE_EQ(world.position.x(), 3.0);
	EXPECT_DOUBLE_EQ(world.position.y(), -2.0);
	EXPECT_DOUBLE_EQ(world.position.z(), 7.0);
	EXPECT_TRUE(world.m_synced);
}

TEST_F(WorldPositionTest, AnEditedWorldPositionMovesTheTransform) {
	Scene scene;
	auto entity = scene.createEntity("Ship");
	auto& world = entity.addComponent<component::WorldPosition>();
	world.position = {10.0, 20.0, 30.0};
	scene.syncWorldPositions();
	EXPECT_FLOAT_EQ(translationOf(entity).x(), 10.f);
	EXPECT_FLOAT_EQ(translationOf(entity).y(), 20.f);
	EXPECT_FLOAT_EQ(translationOf(entity).z(), 30.f);
}

TEST_F(WorldPositionTest, AMovedTransformUpdatesTheWorldPosition) {
	Scene scene;
	auto entity = scene.createEntity("Ship");
	const auto& world = entity.addComponent<component::WorldPosition>();
	scene.shiftWorldOrigin({1000.0, 0.0, 0.0});
	translationOf(entity) = {5.f, 1.f, 0.f};
	scene.syncWorldPositions();
	EXPECT_DOUBLE_EQ(world.position.x(), 1005.0);
	EXPECT_DOUBLE_EQ(world.position.y(), 1.0);
}

TEST_F(WorldPositionTest, AShiftPlacesFarEntitiesWithoutFloatRounding) {
	Scene scene;
	auto station = scene.createEntity("Station");
	auto& world = station.addComponent<component::WorldPosition>();
	// 1.5e11 m is one astronomical unit: a float steps by 16 km there, a double by 30 micrometres.
	world.position = {1.5e11 + 0.123, 0.0, -0.456};
	scene.syncWorldPositions();
	scene.shiftWorldOrigin({1.5e11, 0.0, 0.0});
	EXPECT_NEAR(translationOf(station).x(), 0.123f, 1e-4f);
	EXPECT_NEAR(translationOf(station).z(), -0.456f, 1e-6f);
	EXPECT_DOUBLE_EQ(world.position.x(), 1.5e11 + 0.123);
}

TEST_F(WorldPositionTest, AShiftTranslatesTheOtherRootsAndLeavesTheChildren) {
	Scene scene;
	auto root = scene.createEntity("Root");
	translationOf(root) = {100.f, 0.f, 0.f};
	auto child = scene.createEntity("Child");
	scene.setParent(child, root);
	translationOf(child) = {1.f, 2.f, 3.f};
	scene.shiftWorldOrigin({40.0, 0.0, -10.0});
	EXPECT_FLOAT_EQ(translationOf(root).x(), 60.f);
	EXPECT_FLOAT_EQ(translationOf(root).z(), 10.f);
	EXPECT_FLOAT_EQ(translationOf(child).x(), 1.f);
	EXPECT_FLOAT_EQ(translationOf(child).z(), 3.f);
	EXPECT_DOUBLE_EQ(scene.getWorldOrigin().x(), 40.0);
}

TEST_F(WorldPositionTest, TheOriginFollowsTheCameraBeyondTheShiftRadius) {
	auto scene = mkShared<Scene>();
	scene->getFloatingOriginSettings() = {.enabled = true, .shiftRadius = 100.0};
	auto camera = scene->createEntity("Camera");
	camera.addComponent<component::Camera>().primary = true;
	auto ship = scene->createEntity("Ship");
	ship.addComponent<component::WorldPosition>().position = {250.0, 0.0, 0.0};
	scene->onStartRuntime();
	translationOf(camera) = {50.f, 0.f, 0.f};
	run(*scene, 1);
	EXPECT_DOUBLE_EQ(scene->getWorldOrigin().x(), 0.0);
	translationOf(camera) = {240.f, 0.f, 0.f};
	run(*scene, 1);
	EXPECT_DOUBLE_EQ(scene->getWorldOrigin().x(), 240.0);
	EXPECT_FLOAT_EQ(translationOf(camera).x(), 0.f);
	EXPECT_FLOAT_EQ(translationOf(ship).x(), 10.f);
	scene->onEndRuntime();
}

TEST_F(WorldPositionTest, ADisabledFloatingOriginNeverMoves) {
	auto scene = mkShared<Scene>();
	auto camera = scene->createEntity("Camera");
	camera.addComponent<component::Camera>().primary = true;
	scene->onStartRuntime();
	translationOf(camera) = {1e6f, 0.f, 0.f};
	run(*scene, 1);
	EXPECT_DOUBLE_EQ(scene->getWorldOrigin().x(), 0.0);
	scene->onEndRuntime();
}

TEST_F(WorldPositionTest, SettingsAreClamped) {
	const FloatingOriginSettings small{.enabled = true, .shiftRadius = 0.1};
	EXPECT_DOUBLE_EQ(small.clamped().shiftRadius, FloatingOriginSettings::minShiftRadius);
	const FloatingOriginSettings invalid{.enabled = true, .shiftRadius = std::numeric_limits<double>::infinity()};
	EXPECT_DOUBLE_EQ(invalid.clamped().shiftRadius, FloatingOriginSettings{}.shiftRadius);
}

TEST_F(WorldPositionTest, SceneFileKeepsTheOriginAndTheDoublePositions) {
	const auto scene = mkShared<Scene>();
	scene->getFloatingOriginSettings() = {.enabled = true, .shiftRadius = 512.0};
	auto ship = scene->createEntityWithUUID(7, "Ship");
	ship.addComponent<component::WorldPosition>().position = {1.5e11 + 0.125, -3.0e9, 42.0};
	scene->syncWorldPositions();
	scene->shiftWorldOrigin({1.5e11, -3.0e9, 0.0});
	const auto file = std::filesystem::temp_directory_path() / "worldPosition.owl";
	ASSERT_TRUE(SceneSerializer(scene).serialize(file));
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserialize(file));
	std::filesystem::remove(file);
	EXPECT_TRUE(loaded->getFloatingOriginSettings().enabled);
	EXPECT_DOUBLE_EQ(loaded->getFloatingOriginSettings().shiftRadius, 512.0);
	EXPECT_DOUBLE_EQ(loaded->getWorldOrigin().x(), 1.5e11);
	EXPECT_DOUBLE_EQ(loaded->getWorldOrigin().y(), -3.0e9);
	const auto copy = loaded->findEntityByUUID(7);
	ASSERT_TRUE(copy);
	ASSERT_TRUE(copy.hasComponent<component::WorldPosition>());
	EXPECT_DOUBLE_EQ(copy.getComponent<component::WorldPosition>().position.x(), 1.5e11 + 0.125);
	loaded->syncWorldPositions();
	EXPECT_NEAR(translationOf(copy).x(), 0.125f, 1e-6f);
	EXPECT_NEAR(translationOf(copy).z(), 42.f, 1e-5f);
}

TEST_F(WorldPositionTest, CopyKeepsTheOriginAndItsSettings) {
	const auto scene = mkShared<Scene>();
	scene->getFloatingOriginSettings().enabled = true;
	scene->shiftWorldOrigin({5.0, 6.0, 7.0});
	const auto copy = Scene::copy(scene);
	EXPECT_TRUE(copy->getFloatingOriginSettings().enabled);
	EXPECT_DOUBLE_EQ(copy->getWorldOrigin().z(), 7.0);
}

namespace {

// Runs an oscillator, a patrol and a parallax drift, optionally moving the origin half way, and returns where they end.
auto runBehaviours(const math::vec3d& iShift) -> std::vector<math::vec3> {
	auto scene = mkShared<Scene>();
	auto camera = scene->createEntity("Camera");
	camera.addComponent<component::Camera>().primary = true;
	auto bobber = scene->createEntity("Bobber");
	bobber.getComponent<component::Transform>().transform.translation() = {10.f, 0.f, 0.f};
	bobber.addComponent<component::Oscillator>();
	auto guard = scene->createEntity("Guard");
	auto& patrol = guard.addComponent<component::Patrol>();
	patrol.waypoints = {{0.f, 5.f, 0.f}, {6.f, 5.f, 0.f}};
	patrol.speed = 2.f;
	patrol.waitTime = 0.f;
	auto cloud = scene->createEntity("Cloud");
	cloud.getComponent<component::Transform>().transform.translation() = {0.f, -3.f, 0.f};
	cloud.addComponent<component::Drift>().cameraFollow = 0.5f;
	scene->onStartRuntime();
	core::Timestep step;
	const auto run = [&scene, &step](const int iFrames) -> void {
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(20));
			scene->onUpdateRuntime(step, false);
		}
	};
	run(25);
	scene->shiftWorldOrigin(iShift);
	run(40);
	std::vector<math::vec3> ends;
	for (const auto& entity: {bobber, guard, cloud}) {
		const auto& translation = entity.getComponent<component::Transform>().transform.translation();
		ends.emplace_back(translation.x() + static_cast<float>(iShift.x()),
						  translation.y() + static_cast<float>(iShift.y()), translation.z());
	}
	scene->onEndRuntime();
	return ends;
}

}// namespace

TEST_F(WorldPositionTest, BehavioursCarryOnAcrossAShift) {
	const auto reference = runBehaviours({0.0, 0.0, 0.0});
	const auto shifted = runBehaviours({300.0, -40.0, 0.0});
	ASSERT_EQ(reference.size(), shifted.size());
	for (std::size_t i = 0; i < reference.size(); ++i) {
		EXPECT_NEAR(shifted[i].x(), reference[i].x(), 1e-3f) << i;
		EXPECT_NEAR(shifted[i].y(), reference[i].y(), 1e-3f) << i;
	}
}
