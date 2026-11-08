/**
 * @file NavMesh_test.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <math/matrixCreation.h>
#include <scene/nav/Avoidance.h>
#include <scene/nav/NavMesh.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace owl;
using namespace owl::scene::nav;

namespace {

auto at(const math::vec3& iPosition) -> math::mat4 { return math::translate(math::identity<float, 4>(), iPosition); }

// A slab of `iSize` whose top is at `iTop`, centred on (x, z).
void addSlab(NavGeometry& ioGeometry, const math::vec3& iCentre, const math::vec3& iSize) {
	ioGeometry.addBox(iSize, at({iCentre.x(), iCentre.y() - iSize.y() * 0.5f, iCentre.z()}));
}

// A ramp from (x0, y0) to (x1, y1) along X, `iWidth` wide around z = 0, upward normal.
void addRamp(NavGeometry& ioGeometry, const float iX0, const float iY0, const float iX1, const float iY1,
			 const float iWidth) {
	const float half = iWidth * 0.5f;
	const std::vector<math::vec3> corners{{iX0, iY0, -half}, {iX0, iY0, half}, {iX1, iY1, half}, {iX1, iY1, -half}};
	const std::vector<uint32_t> faces{0, 1, 2, 0, 2, 3};
	ioGeometry.addTriangles(corners, faces, math::identity<float, 4>());
}

auto pathLength(const std::vector<NavPathPoint>& iPath) -> float {
	float length = 0.f;
	for (size_t index = 1; index < iPath.size(); ++index)
		length += (iPath[index].position - iPath[index - 1].position).norm();
	return length;
}

auto settings() -> NavMeshSettings {
	NavMeshSettings result;
	result.cellSize = 0.2f;
	result.cellHeight = 0.1f;
	return result;
}

class NavMeshTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

}// namespace

TEST_F(NavMeshTest, AFlatFloorIsOneSurfaceShrunkByTheAgentRadius) {
	NavGeometry geometry;
	addSlab(geometry, {0.f, 0.f, 0.f}, {10.f, 0.2f, 10.f});
	NavMesh mesh;
	ASSERT_TRUE(mesh.build(geometry, settings(), {-6.f, -1.f, -6.f}, {6.f, 3.f, 6.f}).has_value());
	EXPECT_FALSE(mesh.isEmpty());
	const float inner = 10.f - 2.f * settings().agentRadius;
	EXPECT_NEAR(mesh.getArea(), inner * inner, inner * inner * 0.08f);
	for (const auto& vertex: mesh.getVertices()) EXPECT_NEAR(vertex.y(), 0.f, 0.11f);
	std::vector<NavPathPoint> path;
	ASSERT_TRUE(mesh.findPath({-4.f, 0.f, -4.f}, {4.f, 0.f, 4.f}, path));
	ASSERT_EQ(path.size(), 2u);
	EXPECT_NEAR(pathLength(path), std::sqrt(128.f), 0.05f);
}

TEST_F(NavMeshTest, APillarIsWalkedAround) {
	NavGeometry geometry;
	addSlab(geometry, {0.f, 0.f, 0.f}, {12.f, 0.2f, 12.f});
	geometry.addBox({2.f, 3.f, 2.f}, at({0.f, 1.5f, 0.f}));
	NavMesh mesh;
	ASSERT_TRUE(mesh.build(geometry, settings(), {-7.f, -1.f, -7.f}, {7.f, 4.f, 7.f}).has_value());
	EXPECT_FALSE(mesh.findNearest({0.f, 3.f, 0.f}, 0.5f).has_value());
	std::vector<NavPathPoint> path;
	ASSERT_TRUE(mesh.findPath({-4.f, 0.f, 0.1f}, {4.f, 0.f, 0.1f}, path));
	EXPECT_GT(path.size(), 2u);
	EXPECT_GT(pathLength(path), 8.f);
	EXPECT_LT(pathLength(path), 10.f);
	for (const auto& point: path)
		EXPECT_FALSE(std::abs(point.position.x()) < 1.2f && std::abs(point.position.z()) < 1.2f);
}

TEST_F(NavMeshTest, StackedFloorsStayApartUntilARampJoinsThem) {
	NavGeometry geometry;
	addSlab(geometry, {0.f, 0.f, 0.f}, {16.f, 0.2f, 8.f});
	addSlab(geometry, {4.f, 3.f, 0.f}, {8.f, 0.2f, 8.f});
	NavMesh mesh;
	ASSERT_TRUE(mesh.build(geometry, settings(), {-9.f, -1.f, -5.f}, {9.f, 6.f, 5.f}).has_value());
	const auto low = mesh.findNearest({4.f, 0.f, 0.f}, 0.5f);
	const auto high = mesh.findNearest({4.f, 3.f, 0.f}, 0.5f);
	ASSERT_TRUE(low.has_value());
	ASSERT_TRUE(high.has_value());
	EXPECT_NEAR(low->position.y(), 0.f, 0.11f);
	EXPECT_NEAR(high->position.y(), 3.f, 0.11f);
	std::vector<NavPathPoint> path;
	EXPECT_FALSE(mesh.findPath({-6.f, 0.f, 0.f}, {6.f, 3.f, 0.f}, path));

	// A ramp of about 27 degrees from the ground up to the platform, its foot on the ground.
	addRamp(geometry, -6.f, 0.f, 0.f, 3.f, 3.f);
	ASSERT_TRUE(mesh.build(geometry, settings(), {-9.f, -1.f, -5.f}, {9.f, 6.f, 5.f}).has_value());
	ASSERT_TRUE(mesh.findPath({-7.f, 0.f, 3.f}, {6.f, 3.f, 0.f}, path));
	EXPECT_NEAR(path.back().position.y(), 3.f, 0.11f);
}

TEST_F(NavMeshTest, ASteepSlopeIsNotWalkable) {
	NavGeometry geometry;
	addRamp(geometry, 0.f, 0.f, 2.f, 4.f, 4.f);
	NavMesh mesh;
	const auto built = mesh.build(geometry, settings(), {-1.f, -1.f, -3.f}, {3.f, 5.f, 3.f});
	ASSERT_FALSE(built.has_value());
	EXPECT_EQ(built.error(), NavBuildError::NoWalkableArea);
}

TEST_F(NavMeshTest, BadInputsAreRefused) {
	NavMesh mesh;
	NavGeometry geometry;
	EXPECT_EQ(mesh.build(geometry, settings(), {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}).error(), NavBuildError::NoGeometry);
	addSlab(geometry, {0.f, 0.f, 0.f}, {4.f, 0.2f, 4.f});
	EXPECT_EQ(mesh.build(geometry, settings(), {0.f, 0.f, 0.f}, {0.f, 1.f, 1.f}).error(),
			  NavBuildError::InvalidSettings);
	EXPECT_EQ(mesh.build(geometry, settings(), {20.f, 20.f, 20.f}, {21.f, 21.f, 21.f}).error(),
			  NavBuildError::NoGeometry);
	NavMeshSettings tiny = settings();
	tiny.cellSize = 0.001f;
	EXPECT_EQ(mesh.build(geometry, tiny, {-3.f, -1.f, -3.f}, {3.f, 1.f, 3.f}).error(), NavBuildError::TooLarge);
	EXPECT_TRUE(mesh.isEmpty());
}

TEST_F(NavMeshTest, ALinkJoinsTwoFloorsAndIsCrossedInTheMiddleOfThePath) {
	NavGeometry geometry;
	addSlab(geometry, {-4.f, 0.f, 0.f}, {6.f, 0.2f, 6.f});
	addSlab(geometry, {4.f, 1.5f, 0.f}, {6.f, 0.2f, 6.f});
	NavMesh mesh;
	ASSERT_TRUE(mesh.build(geometry, settings(), {-8.f, -1.f, -4.f}, {8.f, 4.f, 4.f}).has_value());
	std::vector<NavPathPoint> path;
	EXPECT_FALSE(mesh.findPath({-5.f, 0.f, 0.f}, {5.f, 1.5f, 0.f}, path));
	const NavMeshLink jump{.start = {-1.5f, 0.f, 0.f},
						   .end = {1.5f, 1.5f, 0.f},
						   .bidirectional = false,
						   .costScale = 2.f};
	mesh.setLinks(std::vector{jump});
	ASSERT_EQ(mesh.getLinks().size(), 1u);
	ASSERT_TRUE(mesh.findPath({-5.f, 0.f, 0.f}, {5.f, 1.5f, 0.f}, path));
	const auto linkCorner =
			std::ranges::find_if(path, [](const NavPathPoint& iPoint) -> bool { return iPoint.link == 0; });
	ASSERT_NE(linkCorner, path.end());
	EXPECT_NEAR(linkCorner->position.x(), -1.5f, 0.3f);
	EXPECT_NEAR((linkCorner + 1)->position.x(), 1.5f, 0.3f);
	EXPECT_NEAR(path.back().position.y(), 1.5f, 0.11f);
	// One way only.
	EXPECT_FALSE(mesh.findPath({5.f, 1.5f, 0.f}, {-5.f, 0.f, 0.f}, path));
	// A link off the mesh is skipped.
	mesh.setLinks(std::vector{NavMeshLink{.start = {0.f, 10.f, 0.f}, .end = {1.f, 10.f, 0.f}}});
	EXPECT_TRUE(mesh.getLinks().empty());
}

TEST_F(NavMeshTest, SavedTrianglesGiveTheSameMesh) {
	NavGeometry geometry;
	addSlab(geometry, {0.f, 0.f, 0.f}, {10.f, 0.2f, 10.f});
	geometry.addCylinder(2.f, 2.f, at({1.f, 1.f, 1.f}));
	NavMesh mesh;
	ASSERT_TRUE(mesh.build(geometry, settings(), {-6.f, -1.f, -6.f}, {6.f, 3.f, 6.f}).has_value());
	NavMesh copy;
	copy.setTriangles(mesh.getVertices(), mesh.getIndices());
	ASSERT_EQ(copy.getTriangles().size(), mesh.getTriangles().size());
	for (size_t index = 0; index < mesh.getTriangles().size(); ++index)
		EXPECT_EQ(copy.getTriangles()[index].neighbours, mesh.getTriangles()[index].neighbours);
	EXPECT_EQ(copy.getBorderEdges().size(), mesh.getBorderEdges().size());
	std::vector<NavPathPoint> first;
	std::vector<NavPathPoint> second;
	ASSERT_TRUE(mesh.findPath({-4.f, 0.f, 3.f}, {4.f, 0.f, -2.f}, first));
	ASSERT_TRUE(copy.findPath({-4.f, 0.f, 3.f}, {4.f, 0.f, -2.f}, second));
	ASSERT_EQ(first.size(), second.size());
	for (size_t index = 0; index < first.size(); ++index) EXPECT_EQ(first[index].position, second[index].position);
}

TEST_F(NavMeshTest, MovingAlongTheSurfaceSlidesOnTheWalls) {
	NavGeometry geometry;
	addSlab(geometry, {0.f, 0.f, 0.f}, {10.f, 0.2f, 10.f});
	NavMesh mesh;
	ASSERT_TRUE(mesh.build(geometry, settings(), {-6.f, -1.f, -6.f}, {6.f, 3.f, 6.f}).has_value());
	const auto start = mesh.findNearest({0.f, 0.f, 0.f});
	ASSERT_TRUE(start.has_value());
	const auto inside = mesh.moveAlongSurface(*start, {2.f, 0.f, 1.f});
	EXPECT_NEAR(inside.position.x(), 2.f, 1e-4f);
	EXPECT_NEAR(inside.position.z(), 1.f, 1e-4f);
	// Through the wall at x = 4.6: stopped on it, sliding up along it.
	const auto blocked = mesh.moveAlongSurface(inside, {9.f, 0.f, 3.f});
	EXPECT_LT(blocked.position.x(), 4.7f);
	EXPECT_GT(blocked.position.x(), 4.3f);
	EXPECT_GT(blocked.position.z(), 1.f);
	EXPECT_TRUE(mesh.findNearest(blocked.position, 0.01f).has_value());
}

TEST(NavAvoidance, TwoBodiesHeadOnSwapWithoutTouching) {
	std::vector<AvoidanceBody> bodies{{.position = {-4.f, 0.f},
									   .velocity = {},
									   .preferred = {},
									   .radius = 0.4f,
									   .maxSpeed = 2.f,
									   .responsive = true},
									  {.position = {4.f, 0.f},
									   .velocity = {},
									   .preferred = {},
									   .radius = 0.4f,
									   .maxSpeed = 2.f,
									   .responsive = true}};
	const std::vector<math::vec2> goals{{4.f, 0.f}, {-4.f, 0.f}};
	float closest = 100.f;
	constexpr float step = 1.f / 60.f;
	for (int tick = 0; tick < 600; ++tick) {
		for (size_t index = 0; index < bodies.size(); ++index) {
			const math::vec2 offset = goals[index] - bodies[index].position;
			const float distance = offset.norm();
			bodies[index].preferred =
					distance > 1e-3f ? offset * (std::min(2.f, distance / step) / distance) : math::vec2{};
		}
		const auto velocities = computeAvoidance(bodies, {}, step);
		for (size_t index = 0; index < bodies.size(); ++index) {
			bodies[index].velocity = velocities[index];
			bodies[index].position += velocities[index] * step;
		}
		closest = std::min(closest, (bodies[0].position - bodies[1].position).norm());
	}
	EXPECT_GT(closest, 0.75f);
	EXPECT_LT((bodies[0].position - goals[0]).norm(), 0.05f);
	EXPECT_LT((bodies[1].position - goals[1]).norm(), 0.05f);
}

TEST(NavAvoidance, AnObstacleKeepsItsCourseAndIsAvoided) {
	std::vector<AvoidanceBody> bodies{{.position = {-3.f, 0.f},
									   .velocity = {},
									   .preferred = {},
									   .radius = 0.4f,
									   .maxSpeed = 2.f,
									   .responsive = true},
									  {.position = {0.f, 0.f},
									   .velocity = {},
									   .preferred = {},
									   .radius = 1.f,
									   .maxSpeed = 0.f,
									   .responsive = false}};
	float closest = 100.f;
	constexpr float step = 1.f / 60.f;
	for (int tick = 0; tick < 300; ++tick) {
		const math::vec2 offset = math::vec2{3.f, 0.f} - bodies[0].position;
		bodies[0].preferred = offset.norm() > 1e-3f ? offset.normalized() * 2.f : math::vec2{};
		const auto velocities = computeAvoidance(bodies, {}, step);
		EXPECT_EQ(velocities[1], bodies[1].velocity);
		bodies[0].velocity = velocities[0];
		bodies[0].position += velocities[0] * step;
		closest = std::min(closest, bodies[0].position.norm());
	}
	EXPECT_GT(closest, 1.35f);
	EXPECT_GT(bodies[0].position.x(), 2.f);
}

TEST_F(NavMeshTest, ARoomOfFurnitureKeepsTheFloorAroundIt) {
	NavGeometry geometry;
	addSlab(geometry, {0.f, 0.f, 0.f}, {8.f, 0.2f, 8.f});
	const std::vector<math::vec3> crates{{-2.f, 0.f, -2.f}, {1.f, 0.f, -2.5f}, {2.5f, 0.f, 1.f},
										 {-1.5f, 0.f, 2.f}, {0.5f, 0.f, 0.5f}, {-2.8f, 0.f, 0.3f}};
	for (const auto& crate: crates) geometry.addBox({0.6f, 0.45f, 0.6f}, at(crate + math::vec3{0.f, 0.225f, 0.f}));
	NavMeshSettings tuned = settings();
	tuned.cellSize = 0.08f;
	tuned.cellHeight = 0.04f;
	tuned.agentRadius = 0.15f;
	tuned.maxClimb = 0.2f;
	tuned.minRegionArea = 0.2f;
	NavMesh mesh;
	ASSERT_TRUE(mesh.build(geometry, tuned, {-4.5f, -0.5f, -4.5f}, {4.5f, 2.5f, 4.5f}).has_value());
	const float floor = 7.7f * 7.7f;
	const float crate = 0.9f * 0.9f;
	EXPECT_NEAR(mesh.getArea(), floor - 6.f * crate, floor * 0.05f);
	std::vector<NavPathPoint> path;
	EXPECT_TRUE(mesh.findPath({-3.5f, 0.f, -3.5f}, {3.5f, 0.f, 3.5f}, path));
	EXPECT_TRUE(mesh.findPath({-3.5f, 0.f, 3.5f}, {3.5f, 0.f, -3.5f}, path));
}
