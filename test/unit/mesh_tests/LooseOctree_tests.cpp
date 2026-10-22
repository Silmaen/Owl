/**
 * @file LooseOctree_tests.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/geometry/LooseOctree.h>
#include <math/matrixCreation.h>
#include <math/trigonometry.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

using namespace owl;
using data::geometry::LooseOctree;

namespace {
auto cube(const math::vec3& iCentre, const float iHalf) -> math::box3f {
	const math::vec3 extent{iHalf, iHalf, iHalf};
	return math::box3f{iCentre - extent, iCentre + extent};
}

auto sorted(std::vector<uint32_t> iIds) -> std::vector<uint32_t> {
	std::ranges::sort(iIds);
	return iIds;
}
}// namespace

TEST(LooseOctree, InsertsQueriesAndRemoves) {
	LooseOctree tree{{.centre = {0.f, 0.f, 0.f}, .halfSize = 16.f, .maxDepth = 4}};
	tree.insert(0, cube({1.f, 1.f, 1.f}, 0.5f));
	tree.insert(3, cube({-10.f, 2.f, 0.f}, 0.5f));
	tree.insert(7, cube({10.f, -10.f, 10.f}, 4.f));
	EXPECT_EQ(tree.size(), 3u);
	EXPECT_TRUE(tree.contains(3));
	EXPECT_FALSE(tree.contains(2));
	EXPECT_FALSE(tree.contains(100));

	std::vector<uint32_t> ids;
	tree.query(cube({0.f, 0.f, 0.f}, 2.f), ids);
	EXPECT_EQ(sorted(ids), (std::vector<uint32_t>{0}));
	ids.clear();
	tree.query(cube({0.f, 0.f, 0.f}, 20.f), ids);
	EXPECT_EQ(sorted(ids), (std::vector<uint32_t>{0, 3, 7}));

	tree.remove(3);
	tree.remove(3);
	EXPECT_EQ(tree.size(), 2u);
	ids.clear();
	tree.query(cube({0.f, 0.f, 0.f}, 20.f), ids);
	EXPECT_EQ(sorted(ids), (std::vector<uint32_t>{0, 7}));
}

TEST(LooseOctree, MovesAndReleasesEmptyNodes) {
	LooseOctree tree{{.centre = {0.f, 0.f, 0.f}, .halfSize = 16.f, .maxDepth = 5}};
	tree.insert(1, cube({12.f, 12.f, 12.f}, 0.2f));
	const size_t deepNodes = tree.getNodeCount();
	EXPECT_GT(deepNodes, 1u);
	tree.insert(1, cube({-12.f, -12.f, -12.f}, 0.2f));
	EXPECT_EQ(tree.getNodeCount(), deepNodes);
	EXPECT_NEAR(tree.getBox(1).min().x(), -12.2f, 1e-5f);
	std::vector<uint32_t> ids;
	tree.query(cube({12.f, 12.f, 12.f}, 1.f), ids);
	EXPECT_TRUE(ids.empty());
	tree.remove(1);
	EXPECT_EQ(tree.getNodeCount(), 1u);
	tree.clear();
	EXPECT_EQ(tree.size(), 0u);
}

TEST(LooseOctree, GrowsTheRootForAFarBox) {
	LooseOctree tree{{.centre = {0.f, 0.f, 0.f}, .halfSize = 4.f, .maxDepth = 3}};
	tree.insert(0, cube({1.f, 1.f, 1.f}, 0.5f));
	tree.insert(1, cube({500.f, -300.f, 20.f}, 1.f));
	EXPECT_GE(tree.getRootHalfSize(), 128.f);
	std::vector<uint32_t> ids;
	tree.query(cube({500.f, -300.f, 20.f}, 2.f), ids);
	EXPECT_EQ(ids, (std::vector<uint32_t>{1}));
	ids.clear();
	tree.query(cube({1.f, 1.f, 1.f}, 0.1f), ids);
	EXPECT_EQ(ids, (std::vector<uint32_t>{0}));
}

TEST(LooseOctree, FrustumQueryMatchesABruteForceTest) {
	LooseOctree tree{{.centre = {0.f, 0.f, 0.f}, .halfSize = 64.f, .maxDepth = 6}};
	std::mt19937 random{42};// NOLINT(bugprone-random-generator-seed): a fixed seed keeps the boxes reproducible
	std::uniform_real_distribution<float> position{-120.f, 120.f};
	std::uniform_real_distribution<float> size{0.1f, 6.f};
	std::vector<math::box3f> boxes;
	for (uint32_t id = 0; id < 2000; ++id) {
		boxes.push_back(cube({position(random), position(random), position(random)}, size(random)));
		tree.insert(id, boxes.back());
	}
	const auto frustum = math::Frustum::fromViewProjection(math::perspective(math::radians(60.f), 1.5f, 0.5f, 80.f));
	std::vector<uint32_t> expected;
	for (uint32_t id = 0; id < boxes.size(); ++id) {
		if (frustum.intersects(boxes[id]))
			expected.push_back(id);
	}
	std::vector<uint32_t> ids;
	tree.query(frustum, ids);
	EXPECT_EQ(sorted(ids), expected);
	const auto& stats = tree.getLastQueryStatistics();
	EXPECT_EQ(stats.hits, expected.size());
	EXPECT_LT(stats.boxesTested, boxes.size());
}

TEST(LooseOctree, HoldsFlatBoxes) {
	LooseOctree tree;
	tree.insert(4, math::box3f{math::vec3{-1.f, -1.f, 0.f}, math::vec3{1.f, 1.f, 0.f}});
	tree.insert(5, math::box3f{math::vec3{30.f, 30.f, 0.f}, math::vec3{31.f, 31.f, 0.f}});
	const auto frustum = math::Frustum::fromViewProjection(math::ortho(-5.f, 5.f, -5.f, 5.f, -1.f, 1.f));
	std::vector<uint32_t> ids;
	tree.query(frustum, ids);
	EXPECT_EQ(ids, (std::vector<uint32_t>{4}));
}
