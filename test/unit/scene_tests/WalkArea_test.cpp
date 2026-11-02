/**
 * @file WalkArea_test.cpp
 * @author Silmaen
 * @date 31/10/2026
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
#include <scene/nav/WalkMesh.h>
#include <scene/nav/WalkQueries.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <vector>

using namespace owl;
using namespace owl::scene;
using nav::WalkMesh;
using nav::WalkPolygon;

namespace {

auto rectangle(const float iX0, const float iY0, const float iX1, const float iY1) -> std::vector<math::vec2> {
	return {{iX0, iY0}, {iX1, iY0}, {iX1, iY1}, {iX0, iY1}};
}

auto pathLength(const std::vector<math::vec2>& iPath) -> float {
	float length = 0.f;
	for (size_t index = 1; index < iPath.size(); ++index) length += (iPath[index] - iPath[index - 1]).norm();
	return length;
}

auto build(const std::vector<WalkPolygon>& iPolygons) -> WalkMesh {
	WalkMesh mesh;
	mesh.build(iPolygons);
	return mesh;
}

// Every segment of the path stays on the floor (checked on samples along it).
auto staysOnFloor(const WalkMesh& iMesh, const std::vector<math::vec2>& iPath) -> bool {
	for (size_t index = 1; index < iPath.size(); ++index) {
		for (int step = 0; step <= 20; ++step) {
			const float share = static_cast<float>(step) / 20.f;
			if (!iMesh.contains(iPath[index - 1] + (iPath[index] - iPath[index - 1]) * share))
				return false;
		}
	}
	return true;
}

class WalkAreaTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames, const int iMilliseconds = 100) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(iMilliseconds));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto translationOf(const Entity& iEntity) -> math::vec3 {
		return iEntity.getComponent<component::Transform>().transform.translation();
	}
};

}// namespace

TEST(WalkMesh, ConvexAndConcaveFloorsCoverTheirArea) {
	const auto square = build({{.outline = rectangle(0.f, 0.f, 4.f, 2.f), .holes = {}}});
	EXPECT_EQ(square.getTriangles().size(), 2u);
	EXPECT_NEAR(square.getArea(), 8.f, 1e-4f);
	// An L, given clockwise: the winding does not matter.
	const std::vector<math::vec2> shape{{0.f, 0.f}, {0.f, 4.f}, {1.f, 4.f}, {1.f, 1.f}, {4.f, 1.f}, {4.f, 0.f}};
	const auto ell = build({{.outline = shape, .holes = {}}});
	EXPECT_EQ(ell.getTriangles().size(), 4u);
	EXPECT_NEAR(ell.getArea(), 7.f, 1e-4f);
	EXPECT_TRUE(ell.contains({0.5f, 3.f}));
	EXPECT_TRUE(ell.contains({3.f, 0.5f}));
	EXPECT_FALSE(ell.contains({3.f, 3.f}));
	// Each inner edge is shared by two triangles: one floor.
	size_t inner = 0;
	for (const auto& triangle: ell.getTriangles())
		for (const auto neighbour: triangle.neighbours) inner += neighbour >= 0 ? 1u : 0u;
	EXPECT_EQ(inner, 6u);
	EXPECT_EQ(ell.getBorderEdges().size(), 6u);
}

TEST(WalkMesh, HolesAreCutOut) {
	const auto mesh = build({{.outline = rectangle(0.f, 0.f, 10.f, 10.f),
							  .holes = {rectangle(2.f, 2.f, 4.f, 4.f), rectangle(6.f, 5.f, 8.f, 8.f)}}});
	EXPECT_NEAR(mesh.getArea(), 100.f - 4.f - 6.f, 1e-3f);
	EXPECT_FALSE(mesh.contains({3.f, 3.f}));
	EXPECT_FALSE(mesh.contains({7.f, 6.f}));
	EXPECT_TRUE(mesh.contains({5.f, 5.f}));
	EXPECT_TRUE(mesh.contains({1.f, 9.f}));
	EXPECT_EQ(mesh.getBorderEdges().size(), 12u);
}

TEST(WalkMesh, DegenerateInputsAreSkipped) {
	EXPECT_TRUE(build({}).isEmpty());
	EXPECT_TRUE(build({{.outline = {{0.f, 0.f}, {1.f, 0.f}}, .holes = {}}}).isEmpty());
	EXPECT_TRUE(build({{.outline = {{0.f, 0.f}, {1.f, 0.f}, {2.f, 0.f}}, .holes = {}}}).isEmpty());
	// A repeated closing point and a collinear vertex.
	const auto mesh =
			build({{.outline = {{0.f, 0.f}, {1.f, 0.f}, {2.f, 0.f}, {2.f, 2.f}, {0.f, 2.f}, {0.f, 0.f}}, .holes = {}}});
	EXPECT_NEAR(mesh.getArea(), 4.f, 1e-4f);
	std::vector<math::vec2> path;
	EXPECT_FALSE(WalkMesh{}.findPath({0.f, 0.f}, {1.f, 1.f}, path));
	EXPECT_FALSE(WalkMesh{}.getClosestPoint({0.f, 0.f}).has_value());
}

TEST(WalkMesh, AStraightLineWhenNothingIsInTheWay) {
	const auto mesh = build({{.outline = rectangle(0.f, 0.f, 10.f, 4.f), .holes = {}}});
	std::vector<math::vec2> path;
	ASSERT_TRUE(mesh.findPath({1.f, 1.f}, {9.f, 3.f}, path));
	ASSERT_EQ(path.size(), 2u);
	EXPECT_NEAR(path.back().x(), 9.f, 1e-4f);
	EXPECT_NEAR(path.back().y(), 3.f, 1e-4f);
}

TEST(WalkMesh, ThePathTurnsAtTheCornersOfAHole) {
	const auto mesh = build({{.outline = rectangle(0.f, 0.f, 10.f, 10.f), .holes = {rectangle(4.f, 2.f, 6.f, 8.f)}}});
	std::vector<math::vec2> path;
	ASSERT_TRUE(mesh.findPath({2.f, 5.f}, {8.f, 5.f}, path));
	ASSERT_EQ(path.size(), 4u);
	// Around the bottom or the top of the hole, through both of its corners on that side.
	const float side = path[1].y();
	EXPECT_TRUE(std::abs(side - 2.f) < 1e-3f || std::abs(side - 8.f) < 1e-3f);
	EXPECT_NEAR(path[1].x(), 4.f, 1e-3f);
	EXPECT_NEAR(path[2].x(), 6.f, 1e-3f);
	EXPECT_NEAR(path[2].y(), side, 1e-3f);
	EXPECT_NEAR(pathLength(path), 2.f * std::sqrt(4.f + 9.f) + 2.f, 1e-3f);
	EXPECT_TRUE(staysOnFloor(mesh, path));
}

TEST(WalkMesh, ThePathRoundsTheInnerCornerOfAnEll) {
	const std::vector<math::vec2> shape{{0.f, 0.f}, {4.f, 0.f}, {4.f, 1.f}, {1.f, 1.f}, {1.f, 4.f}, {0.f, 4.f}};
	const auto mesh = build({{.outline = shape, .holes = {}}});
	std::vector<math::vec2> path;
	ASSERT_TRUE(mesh.findPath({3.5f, 0.5f}, {0.5f, 3.5f}, path));
	ASSERT_EQ(path.size(), 3u);
	EXPECT_NEAR(path[1].x(), 1.f, 1e-4f);
	EXPECT_NEAR(path[1].y(), 1.f, 1e-4f);
}

TEST(WalkMesh, PointsOffTheFloorAreMovedOntoIt) {
	const auto mesh = build({{.outline = rectangle(0.f, 0.f, 4.f, 4.f), .holes = {rectangle(1.f, 1.f, 3.f, 3.f)}}});
	const auto outside = mesh.getClosestPoint({6.f, 2.f});
	ASSERT_TRUE(outside.has_value());
	EXPECT_NEAR(outside->x(), 4.f, 1e-4f);
	EXPECT_NEAR(outside->y(), 2.f, 1e-4f);
	const auto inHole = mesh.getClosestPoint({2.f, 1.2f});
	ASSERT_TRUE(inHole.has_value());
	EXPECT_NEAR(inHole->y(), 1.f, 1e-4f);
	std::vector<math::vec2> path;
	ASSERT_TRUE(mesh.findPath({0.5f, 0.5f}, {2.f, 2.5f}, path));
	EXPECT_NEAR(path.back().y(), 3.f, 1e-4f);
	EXPECT_TRUE(staysOnFloor(mesh, path));
}

TEST(WalkMesh, FloorsJoinWhenTheyShareAnEdge) {
	const auto joined = build({{.outline = rectangle(0.f, 0.f, 2.f, 2.f), .holes = {}},
							   {.outline = rectangle(2.f, 0.f, 4.f, 2.f), .holes = {}}});
	std::vector<math::vec2> path;
	EXPECT_TRUE(joined.findPath({0.5f, 1.f}, {3.5f, 1.f}, path));
	const auto apart = build({{.outline = rectangle(0.f, 0.f, 2.f, 2.f), .holes = {}},
							  {.outline = rectangle(3.f, 0.f, 5.f, 2.f), .holes = {}}});
	EXPECT_FALSE(apart.findPath({0.5f, 1.f}, {4.f, 1.f}, path));
	EXPECT_TRUE(path.empty());
}

TEST(WalkMesh, RandomPathsStayOnAFloorWithHoles) {
	const auto mesh = build({{.outline = {{0.f, 0.f}, {12.f, 0.f}, {12.f, 6.f}, {8.f, 6.f}, {8.f, 10.f}, {0.f, 10.f}},
							  .holes = {rectangle(2.f, 2.f, 3.f, 7.f),
										rectangle(5.f, 1.f, 6.f, 4.f),
										{{5.f, 6.f}, {7.f, 7.f}, {5.5f, 8.5f}}}}});
	std::mt19937 random{7};// NOLINT(bugprone-random-generator-seed): a fixed seed keeps the random paths reproducible
	std::uniform_real_distribution<float> x{0.f, 12.f};
	std::uniform_real_distribution<float> y{0.f, 10.f};
	int found = 0;
	for (int attempt = 0; attempt < 200; ++attempt) {
		const math::vec2 from{x(random), y(random)};
		const math::vec2 to{x(random), y(random)};
		if (!mesh.contains(from) || !mesh.contains(to))
			continue;
		std::vector<math::vec2> path;
		ASSERT_TRUE(mesh.findPath(from, to, path));
		EXPECT_TRUE(staysOnFloor(mesh, path));
		EXPECT_GE(pathLength(path) + 1e-4f, (to - from).norm());
		++found;
	}
	EXPECT_GT(found, 50);
}

TEST_F(WalkAreaTest, ScaleAndDepthFollowTheHeight) {
	component::WalkArea area;
	EXPECT_FLOAT_EQ(area.getScaleAt(5.f), 1.f);
	EXPECT_FLOAT_EQ(area.getDepthAt(5.f), 0.f);
	area.depthScale = true;
	area.nearY = 0.f;
	area.nearScale = 1.f;
	area.farY = 10.f;
	area.farScale = 0.5f;
	EXPECT_NEAR(area.getScaleAt(5.f), 0.75f, 1e-5f);
	EXPECT_NEAR(area.getScaleAt(-2.f), 1.1f, 1e-5f);
	EXPECT_NEAR(area.getScaleAt(1000.f), 0.01f, 1e-5f);
	area.depthSort = true;
	area.depthPerUnit = 0.1f;
	EXPECT_NEAR(area.getDepthAt(-1.f), 0.1f, 1e-5f);
	EXPECT_NEAR(area.getDepthAt(3.f), -0.3f, 1e-5f);
	EXPECT_EQ(area.getRing(0, 0), nullptr);
}

TEST_F(WalkAreaTest, RoundTripsThroughTheSceneFile) {
	auto scene = mkShared<Scene>();
	auto floor = scene->createEntity("Floor");
	auto& area = floor.addComponent<component::WalkArea>();
	area.polygons.push_back({.outline = rectangle(0.f, 0.f, 8.f, 4.f), .holes = {rectangle(3.f, 1.f, 4.f, 2.f)}});
	area.polygons.push_back({.outline = rectangle(8.f, 0.f, 10.f, 2.f), .holes = {}});
	area.depthScale = true;
	area.nearY = 1.f;
	area.nearScale = 1.2f;
	area.farY = 4.f;
	area.farScale = 0.6f;
	area.depthSort = true;
	area.depthPerUnit = 0.25f;
	auto hero = scene->createEntity("Hero");
	auto& agent = hero.addComponent<component::WalkAgent>();
	agent.area = floor.getUUID();
	agent.speed = 3.5f;
	agent.scaleSpeed = false;
	agent.facing = component::WalkAgent::Facing::TurnY;
	agent.speedParameter = "pace";
	auto pillar = scene->createEntity("Pillar");
	auto& mask = pillar.addComponent<component::WalkBehind>();
	mask.area = floor.getUUID();
	mask.baseline = 2.5f;

	const auto text = SceneSerializer(scene).serializeToString();
	auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer({text.begin(), text.end()}));
	const auto readFloor = loaded->findEntityByUUID(floor.getUUID());
	ASSERT_TRUE(readFloor && readFloor.hasComponent<component::WalkArea>());
	const auto& read = readFloor.getComponent<component::WalkArea>();
	EXPECT_EQ(read.polygons, area.polygons);
	EXPECT_TRUE(read.depthScale);
	EXPECT_FLOAT_EQ(read.nearY, 1.f);
	EXPECT_FLOAT_EQ(read.nearScale, 1.2f);
	EXPECT_FLOAT_EQ(read.farY, 4.f);
	EXPECT_FLOAT_EQ(read.farScale, 0.6f);
	EXPECT_TRUE(read.depthSort);
	EXPECT_FLOAT_EQ(read.depthPerUnit, 0.25f);
	EXPECT_NEAR(read.getMesh().getArea(), 32.f - 1.f + 4.f, 1e-3f);
	const auto readHero = loaded->findEntityByUUID(hero.getUUID());
	ASSERT_TRUE(readHero && readHero.hasComponent<component::WalkAgent>());
	const auto& readAgent = readHero.getComponent<component::WalkAgent>();
	EXPECT_EQ(readAgent.area, floor.getUUID());
	EXPECT_FLOAT_EQ(readAgent.speed, 3.5f);
	EXPECT_FALSE(readAgent.scaleSpeed);
	EXPECT_EQ(readAgent.facing, component::WalkAgent::Facing::TurnY);
	EXPECT_EQ(readAgent.speedParameter, "pace");
	const auto readPillar = loaded->findEntityByUUID(pillar.getUUID());
	ASSERT_TRUE(readPillar && readPillar.hasComponent<component::WalkBehind>());
	EXPECT_EQ(readPillar.getComponent<component::WalkBehind>().area, floor.getUUID());
	EXPECT_FLOAT_EQ(readPillar.getComponent<component::WalkBehind>().baseline, 2.5f);
}

TEST_F(WalkAreaTest, AnAgentWalksAroundAHoleScaledAndSortedByDepth) {
	auto scene = mkShared<Scene>();
	auto floor = scene->createEntity("Floor");
	auto& area = floor.addComponent<component::WalkArea>();
	area.polygons.push_back({.outline = rectangle(0.f, 0.f, 10.f, 10.f), .holes = {rectangle(4.f, 2.f, 6.f, 8.f)}});
	area.depthScale = true;
	area.farY = 10.f;
	area.farScale = 0.5f;
	area.depthSort = true;
	area.depthPerUnit = 0.1f;
	auto hero = scene->createEntity("Hero");
	hero.getComponent<component::Transform>().transform.translation() = {2.f, 5.f, 0.f};
	hero.getComponent<component::Transform>().transform.scale() = {2.f, 2.f, 2.f};
	auto& agent = hero.addComponent<component::WalkAgent>();
	agent.area = floor.getUUID();
	agent.speed = 4.f;
	agent.facing = component::WalkAgent::Facing::MirrorX;
	auto pillar = scene->createEntity("Pillar");
	pillar.addComponent<component::WalkBehind>(component::WalkBehind{.area = floor.getUUID(), .baseline = 3.f});
	scene->onStartRuntime();
	EXPECT_FALSE(nav::walkAgentTo(*scene, pillar, {1.f, 1.f}));
	ASSERT_TRUE(nav::walkAgentTo(*scene, hero, {8.f, 5.f}));
	EXPECT_TRUE(hero.getComponent<component::WalkAgent>().isMoving());
	run(*scene, 1);
	// Halfway up the plane: scale 0.75 of its scale of 2, depth -0.5, and the mask at its baseline depth.
	EXPECT_NEAR(hero.getComponent<component::Transform>().transform.scale().y(), 2.f * 0.75f, 0.1f);
	EXPECT_NEAR(translationOf(pillar).z(), -0.3f, 1e-4f);
	run(*scene, 40);
	EXPECT_FALSE(hero.getComponent<component::WalkAgent>().isMoving());
	EXPECT_NEAR(translationOf(hero).x(), 8.f, 1e-3f);
	EXPECT_NEAR(translationOf(hero).y(), 5.f, 1e-3f);
	EXPECT_NEAR(translationOf(hero).z(), -0.5f, 1e-4f);
	EXPECT_GT(hero.getComponent<component::Transform>().transform.scale().x(), 0.f);
	// Walking left mirrors the sprite.
	ASSERT_TRUE(nav::walkAgentTo(*scene, hero, {8.f, 1.f}));
	run(*scene, 30);
	ASSERT_TRUE(nav::walkAgentTo(*scene, hero, {1.f, 1.f}));
	run(*scene, 2);
	EXPECT_LT(hero.getComponent<component::Transform>().transform.scale().x(), 0.f);
	// Low on the plane, in front: bigger and nearer than the mask.
	run(*scene, 40);
	EXPECT_NEAR(translationOf(hero).y(), 1.f, 1e-3f);
	EXPECT_GT(translationOf(hero).z(), translationOf(pillar).z());
	EXPECT_NEAR(std::abs(hero.getComponent<component::Transform>().transform.scale().x()), 2.f * 0.95f, 1e-3f);
	scene->onEndRuntime();
}

TEST_F(WalkAreaTest, QueriesWorkInWorldSpaceAndAMeshTurnsTheWayItWalks) {
	auto scene = mkShared<Scene>();
	auto floor = scene->createEntity("Floor");
	floor.getComponent<component::Transform>().transform.translation() = {100.f, 50.f, 0.f};
	floor.addComponent<component::WalkArea>().polygons.push_back(
			{.outline = rectangle(0.f, 0.f, 10.f, 4.f), .holes = {}});
	EXPECT_TRUE(nav::isOnWalkArea(*scene, floor, {105.f, 52.f}));
	EXPECT_FALSE(nav::isOnWalkArea(*scene, floor, {5.f, 2.f}));
	const auto closest = nav::getWalkClosestPoint(*scene, floor, {105.f, 60.f});
	ASSERT_TRUE(closest.has_value());
	EXPECT_NEAR(closest->y(), 54.f, 1e-4f);
	EXPECT_FLOAT_EQ(nav::getWalkScaleAt(*scene, floor, {105.f, 52.f}), 1.f);
	std::vector<math::vec2> path;
	ASSERT_TRUE(nav::findWalkPath(*scene, floor, {101.f, 51.f}, {109.f, 53.f}, path));
	EXPECT_NEAR(path.back().x(), 109.f, 1e-4f);
	EXPECT_FALSE(nav::findWalkPath(*scene, Entity{}, {0.f, 0.f}, {1.f, 1.f}, path));
	auto actor = scene->createEntity("Actor");
	actor.getComponent<component::Transform>().transform.translation() = {105.f, 53.f, 0.f};
	auto& agent = actor.addComponent<component::WalkAgent>();
	agent.area = floor.getUUID();
	agent.facing = component::WalkAgent::Facing::TurnY;
	scene->onStartRuntime();
	ASSERT_TRUE(nav::walkAgentTo(*scene, actor, {105.f, 51.f}));
	run(*scene, 1);
	// Walking down the plane, towards the viewer: facing +Z, no turn.
	EXPECT_NEAR(actor.getComponent<component::Transform>().transform.rotation().y(), 0.f, 1e-4f);
	ASSERT_TRUE(nav::walkAgentTo(*scene, actor, {109.f, translationOf(actor).y()}));
	run(*scene, 1);
	EXPECT_NEAR(actor.getComponent<component::Transform>().transform.rotation().y(), std::numbers::pi_v<float> / 2.f,
				1e-3f);
	scene->onEndRuntime();
}
TEST(WalkMesh, AStartOnAnInnerEdgeKeepsTheStraightLine) {
	const auto mesh = build({{.outline = rectangle(0.f, 0.f, 10.f, 10.f), .holes = {rectangle(4.f, 2.f, 6.f, 8.f)}}});
	// (8, 1) lies on the edge between two triangles of this floor.
	std::vector<math::vec2> path;
	ASSERT_TRUE(mesh.findPath({8.f, 1.f}, {1.f, 1.f}, path));
	EXPECT_EQ(path.size(), 2u);
	ASSERT_TRUE(mesh.findPath({1.f, 1.f}, {8.f, 1.f}, path));
	EXPECT_EQ(path.size(), 2u);
}
