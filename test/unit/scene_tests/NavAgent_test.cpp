/**
 * @file NavAgent_test.cpp
 * @author Silmaen
 * @date 08/11/2026
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
#include <scene/nav/NavBake.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class NavAgentTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames, const int iMilliseconds = 50) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(iMilliseconds));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto addStaticBox(Scene& ioScene, const std::string& iName, const math::vec3& iCentre,
							 const math::vec3& iSize) -> Entity {
		auto entity = ioScene.createEntity(iName);
		entity.getComponent<component::Transform>().transform.translation() = iCentre;
		auto& body = entity.addComponent<component::PhysicBody>().body;
		body.type = SceneBody::BodyType::Static;
		body.dimension = SceneBody::Dimension::Space;
		body.shape = SceneBody::Shape::Box;
		body.colliderSize = iSize;
		return entity;
	}

	// A floor of 16 x 16 with a wall across its middle, open at both ends.
	static auto makeRoom(Scene& ioScene) -> Entity {
		addStaticBox(ioScene, "Floor", {0.f, -0.1f, 0.f}, {16.f, 0.2f, 16.f});
		addStaticBox(ioScene, "Wall", {0.f, 1.f, 0.f}, {0.4f, 2.f, 10.f});
		auto volume = ioScene.createEntity("Navigation");
		volume.getComponent<component::Transform>().transform.translation() = {0.f, 1.f, 0.f};
		auto& component = volume.addComponent<component::NavMeshVolume>();
		component.size = {18.f, 6.f, 18.f};
		component.settings.cellSize = 0.2f;
		return volume;
	}

	static auto translationOf(const Entity& iEntity) -> math::vec3 {
		return iEntity.getComponent<component::Transform>().transform.translation();
	}
};

}// namespace

TEST_F(NavAgentTest, ABakeReadsTheStaticCollidersInsideTheVolume) {
	auto scene = mkShared<Scene>();
	const auto volume = makeRoom(*scene);
	auto crate = addStaticBox(*scene, "Crate", {5.f, 0.5f, 5.f}, {1.f, 1.f, 1.f});
	crate.addComponent<component::NavObstacle>();
	addStaticBox(*scene, "Far", {100.f, 0.f, 0.f}, {1.f, 1.f, 1.f});
	auto cart = scene->createEntity("Cart");
	cart.addComponent<component::SplineFollower>();
	auto load = addStaticBox(*scene, "Load", {-5.f, 0.5f, 5.f}, {1.f, 1.f, 1.f});
	scene->setParent(load, cart);
	auto ball = addStaticBox(*scene, "Ball", {-5.f, 0.5f, -5.f}, {1.f, 1.f, 1.f});
	ball.getComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
	const auto geometry = nav::collectNavGeometry(*scene, volume);
	// Floor, wall and the far box: 12 triangles each; the obstacle, the moving cart's load and the ball skipped.
	EXPECT_EQ(geometry.indices.size(), 3u * 12u * 3u);
	ASSERT_TRUE(nav::bakeNavMesh(*scene, volume).has_value());
	const auto& baked = volume.getComponent<component::NavMeshVolume>();
	EXPECT_TRUE(baked.isBaked());
	EXPECT_FALSE(baked.getMesh().findNearest({0.f, 0.f, 0.f}, 0.3f).has_value());
	EXPECT_TRUE(baked.getMesh().findNearest({0.f, 0.f, 7.f}, 0.3f).has_value());
	EXPECT_EQ(nav::findNavVolume(*scene, {1.f, 0.f, 1.f}), volume);
	EXPECT_FALSE(nav::findNavVolume(*scene, {50.f, 0.f, 0.f}));
	// A volume without geometry inside keeps its previous mesh.
	auto empty = scene->createEntity("Empty");
	empty.getComponent<component::Transform>().transform.translation() = {200.f, 0.f, 0.f};
	empty.addComponent<component::NavMeshVolume>().size = {2.f, 2.f, 2.f};
	EXPECT_FALSE(nav::bakeNavMesh(*scene, empty).has_value());
	EXPECT_FALSE(empty.getComponent<component::NavMeshVolume>().isBaked());
	EXPECT_FALSE(nav::bakeNavMesh(*scene, crate).has_value());
}

TEST_F(NavAgentTest, AnAgentWalksAroundTheWallToItsGoal) {
	auto scene = mkShared<Scene>();
	const auto volume = makeRoom(*scene);
	ASSERT_TRUE(nav::bakeNavMesh(*scene, volume).has_value());
	auto hero = scene->createEntity("Hero");
	hero.getComponent<component::Transform>().transform.translation() = {-4.f, 0.f, 0.f};
	auto& agent = hero.addComponent<component::NavAgent>();
	agent.speed = 4.f;
	scene->onStartRuntime();
	auto& running = hero.getComponent<component::NavAgent>();
	running.goTo({4.f, 0.f, 0.f});
	EXPECT_TRUE(running.isMoving());
	float widest = 0.f;
	for (int frame = 0; frame < 200 && running.isMoving(); ++frame) {
		run(*scene, 1);
		widest = std::max(widest, std::abs(translationOf(hero).z()));
		EXPECT_FALSE(std::abs(translationOf(hero).x()) < 0.2f && std::abs(translationOf(hero).z()) < 5.f);
	}
	EXPECT_FALSE(running.isMoving());
	EXPECT_NEAR(translationOf(hero).x(), 4.f, 0.15f);
	EXPECT_NEAR(translationOf(hero).z(), 0.f, 0.15f);
	EXPECT_NEAR(translationOf(hero).y(), 0.f, 0.11f);
	EXPECT_GT(widest, 5.f);
	// Walking +Z, the last turn faced it towards +Z or +X.
	EXPECT_GE(std::cos(hero.getComponent<component::Transform>().transform.rotation().y()), -0.1f);
	// A goal off the mesh is walked to its nearest point; one on a far floor is refused.
	running.goTo({100.f, 0.f, 0.f});
	run(*scene, 1);
	EXPECT_FALSE(running.isMoving());
	scene->onEndRuntime();
}

TEST_F(NavAgentTest, TwoAgentsCrossingAGapKeepApart) {
	auto scene = mkShared<Scene>();
	const auto volume = makeRoom(*scene);
	ASSERT_TRUE(nav::bakeNavMesh(*scene, volume).has_value());
	auto first = scene->createEntity("First");
	first.getComponent<component::Transform>().transform.translation() = {-3.f, 0.f, 6.5f};
	first.addComponent<component::NavAgent>().volume = volume.getUUID();
	auto second = scene->createEntity("Second");
	second.getComponent<component::Transform>().transform.translation() = {3.f, 0.f, 6.5f};
	second.addComponent<component::NavAgent>().volume = volume.getUUID();
	scene->onStartRuntime();
	first.getComponent<component::NavAgent>().goTo({3.f, 0.f, 6.5f});
	second.getComponent<component::NavAgent>().goTo({-3.f, 0.f, 6.5f});
	float closest = 100.f;
	for (int frame = 0; frame < 160; ++frame) {
		run(*scene, 1);
		closest = std::min(closest, (translationOf(first) - translationOf(second)).norm());
	}
	EXPECT_GT(closest, 0.6f);
	EXPECT_LT((translationOf(first) - math::vec3{3.f, 0.f, 6.5f}).norm(), 0.3f);
	EXPECT_LT((translationOf(second) - math::vec3{-3.f, 0.f, 6.5f}).norm(), 0.3f);
	scene->onEndRuntime();
}

TEST_F(NavAgentTest, AnAgentCrossesALinkBetweenTwoFloors) {
	auto scene = mkShared<Scene>();
	addStaticBox(*scene, "Low", {-4.f, -0.1f, 0.f}, {6.f, 0.2f, 6.f});
	addStaticBox(*scene, "High", {4.f, 1.4f, 0.f}, {6.f, 0.2f, 6.f});
	auto volume = scene->createEntity("Navigation");
	volume.getComponent<component::Transform>().transform.translation() = {0.f, 1.f, 0.f};
	volume.addComponent<component::NavMeshVolume>().size = {18.f, 6.f, 8.f};
	ASSERT_TRUE(nav::bakeNavMesh(*scene, volume).has_value());
	auto jump = scene->createEntity("Jump");
	jump.getComponent<component::Transform>().transform.translation() = {-1.5f, 0.f, 0.f};
	jump.addComponent<component::NavLink>().end = {3.f, 1.5f, 0.f};
	auto hero = scene->createEntity("Hero");
	hero.getComponent<component::Transform>().transform.translation() = {-5.f, 0.f, 0.f};
	hero.addComponent<component::NavAgent>();
	scene->onStartRuntime();
	auto& agent = hero.getComponent<component::NavAgent>();
	agent.goTo({5.f, 1.5f, 1.f});
	run(*scene, 120);
	EXPECT_FALSE(agent.isMoving());
	EXPECT_NEAR(translationOf(hero).x(), 5.f, 0.15f);
	EXPECT_NEAR(translationOf(hero).y(), 1.5f, 0.11f);
	EXPECT_EQ(volume.getComponent<component::NavMeshVolume>().getMesh().getLinks().size(), 1u);
	scene->onEndRuntime();
}

TEST_F(NavAgentTest, TheComponentsSurviveASaveAndALoad) {
	auto scene = mkShared<Scene>();
	const auto volume = makeRoom(*scene);
	auto& component = volume.getComponent<component::NavMeshVolume>();
	component.settings.agentRadius = 0.3f;
	component.sources = component::NavMeshVolume::Sources::Colliders;
	ASSERT_TRUE(nav::bakeNavMesh(*scene, volume).has_value());
	auto hero = scene->createEntity("Hero");
	auto& agent = hero.addComponent<component::NavAgent>();
	agent.volume = volume.getUUID();
	agent.speed = 2.5f;
	agent.avoidance = false;
	agent.speedParameter = "pace";
	auto cart = scene->createEntity("Cart");
	cart.addComponent<component::NavObstacle>().radius = 1.2f;
	auto ladder = scene->createEntity("Ladder");
	auto& link = ladder.addComponent<component::NavLink>();
	link.end = {0.f, 3.f, 1.f};
	link.bidirectional = false;
	link.costScale = 3.f;

	const auto text = SceneSerializer(scene).serializeToString();
	auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer({text.begin(), text.end()}));
	const auto& readVolume = loaded->findEntityByUUID(volume.getUUID()).getComponent<component::NavMeshVolume>();
	EXPECT_EQ(readVolume.size, component.size);
	EXPECT_EQ(readVolume.settings, component.settings);
	EXPECT_EQ(readVolume.sources, component::NavMeshVolume::Sources::Colliders);
	EXPECT_EQ(readVolume.vertices, component.vertices);
	EXPECT_EQ(readVolume.indices, component.indices);
	EXPECT_EQ(readVolume.getMesh().getTriangles().size(), component.getMesh().getTriangles().size());
	const auto& readAgent = loaded->findEntityByUUID(hero.getUUID()).getComponent<component::NavAgent>();
	EXPECT_EQ(readAgent.volume, volume.getUUID());
	EXPECT_FLOAT_EQ(readAgent.speed, 2.5f);
	EXPECT_FALSE(readAgent.avoidance);
	EXPECT_EQ(readAgent.speedParameter, "pace");
	EXPECT_FLOAT_EQ(loaded->findEntityByUUID(cart.getUUID()).getComponent<component::NavObstacle>().radius, 1.2f);
	const auto& readLink = loaded->findEntityByUUID(ladder.getUUID()).getComponent<component::NavLink>();
	EXPECT_EQ(readLink.end, link.end);
	EXPECT_FALSE(readLink.bidirectional);
	EXPECT_FLOAT_EQ(readLink.costScale, 3.f);
}
