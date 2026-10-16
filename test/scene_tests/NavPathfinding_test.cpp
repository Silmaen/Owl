/**
 * @file NavPathfinding_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/TilemapAsset.h>
#include <scene/TilemapTerrain.h>
#include <scene/component/Tilemap.h>
#include <scene/nav/NavMap.h>
#include <scene/nav/NavProfile.h>
#include <scene/nav/NavWorld.h>
#include <scene/nav/PathQueue.h>
#include <scene/nav/Pathfinder.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::scene;
using namespace owl::scene::nav;
using owl::scene::component::g_EmptyTileIndex;

namespace {

constexpr int32_t road = 1;
constexpr int32_t slowRoad = 2;
constexpr int32_t eastOnly = 3;

class NavPathfinding : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }

	static void TearDownTestSuite() { core::Log::invalidate(); }

	static auto makeRandom(const uint32_t iSeed) -> std::mt19937 { return std::mt19937{iSeed}; }

	static auto roll(std::mt19937& ioRandom, const uint32_t iCount) -> uint32_t {
		return static_cast<uint32_t>(ioRandom() % iCount);
	}

	static auto makeProfile() -> NavProfile {
		NavProfile profile;
		profile.layer = 0;
		profile.source = NavSource::Overlay;
		profile.rules.push_back({.tile = road});
		profile.rules.push_back({.tile = slowRoad, .cost = 3.f});
		profile.rules.push_back({.tile = eastOnly, .exits = directionBit(Direction::East)});
		return profile;
	}

	static auto makeAsset(const uint32_t iWidth, const uint32_t iHeight) -> TilemapAsset {
		TilemapAsset asset;
		asset.resize(iWidth, iHeight);
		asset.addLayer("roads");
		return asset;
	}

	static void paint(TilemapAsset& ioAsset, const uint32_t iX, const uint32_t iY, const int32_t iTile) {
		auto cell = ioAsset.layers[0].getCell(iX, iY);
		cell.overlay = std::max(iTile, g_EmptyTileIndex);
		ASSERT_TRUE(ioAsset.layers[0].setCell(iX, iY, cell) || cell == ioAsset.layers[0].getCell(iX, iY));
	}

	static void paintRow(TilemapAsset& ioAsset, const uint32_t iFirst, const uint32_t iLast, const uint32_t iY,
						 const int32_t iTile = road) {
		for (uint32_t x = iFirst; x <= iLast; ++x) paint(ioAsset, x, iY, iTile);
	}

	static void paintColumn(TilemapAsset& ioAsset, const uint32_t iX, const uint32_t iFirst, const uint32_t iLast,
							const int32_t iTile = road) {
		for (uint32_t y = iFirst; y <= iLast; ++y) paint(ioAsset, iX, y, iTile);
	}

	static auto makeMap(const TilemapAsset& iAsset, const NavProfile& iProfile) -> NavMap {
		NavMap map;
		map.setProfile(iProfile);
		map.sync(iAsset);
		return map;
	}

	/// Independent Dijkstra over `getEdgeCost`: the cost of the best path, `unreachable` when none.
	static auto referenceCost(const NavMap& iMap, const math::vec2ui& iFrom, const math::vec2ui& iTo) -> uint32_t {
		const auto width = iMap.getSize().x();
		std::vector<uint32_t> best(static_cast<size_t>(width) * iMap.getSize().y(), unreachable);
		using Item = std::pair<uint32_t, uint32_t>;
		std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
		best[iFrom.y() * width + iFrom.x()] = 0;
		open.emplace(0, iFrom.y() * width + iFrom.x());
		while (!open.empty()) {
			const auto [cost, node] = open.top();
			open.pop();
			if (cost != best[node])
				continue;
			if (node == iTo.y() * width + iTo.x())
				return cost;
			for (const auto direction: directions) {
				const auto edge = iMap.getEdgeCost(node % width, node / width, direction);
				if (edge == 0)
					continue;
				const auto next =
						static_cast<uint32_t>(static_cast<int32_t>(node / width) + offsetY(direction)) * width +
						static_cast<uint32_t>(static_cast<int32_t>(node % width) + offsetX(direction));
				if (cost + edge < best[next]) {
					best[next] = cost + edge;
					open.emplace(best[next], next);
				}
			}
		}
		return unreachable;
	}

	/// Sum of the moves of a path, 0 when a move does not exist.
	static auto pathCost(const NavMap& iMap, const PathResult& iPath) -> uint32_t {
		uint32_t total = 0;
		for (size_t i = 1; i < iPath.cells.size(); ++i) {
			const auto& from = iPath.cells[i - 1];
			const auto& to = iPath.cells[i];
			uint32_t edge = 0;
			for (const auto direction: directions) {
				if (static_cast<int64_t>(from.x()) + offsetX(direction) == to.x() &&
					static_cast<int64_t>(from.y()) + offsetY(direction) == to.y())
					edge = iMap.getEdgeCost(from.x(), from.y(), direction);
			}
			if (edge == 0)
				return 0;
			total += edge;
		}
		return total;
	}

	/// A random map: mostly road, some slow road, one-way pieces, holes, and hills.
	static auto makeRandomAsset(const uint32_t iWidth, const uint32_t iHeight, const uint32_t iSeed) -> TilemapAsset {
		std::mt19937 random{iSeed};
		auto asset = makeAsset(iWidth, iHeight);
		for (uint32_t y = 0; y < iHeight; ++y) {
			for (uint32_t x = 0; x < iWidth; ++x) {
				const uint32_t chance = roll(random, 100);
				if (chance < 62)
					paint(asset, x, y, road);
				else if (chance < 72)
					paint(asset, x, y, slowRoad);
				else if (chance < 78)
					paint(asset, x, y, eastOnly);
			}
		}
		for (int hill = 0; hill < 12; ++hill) {
			const uint32_t x = random() % iWidth;
			const uint32_t y = random() % iHeight;
			std::ignore = asset.raiseArea(
					{x, y}, {std::min(x + roll(random, 6), iWidth), std::min(y + roll(random, 6), iHeight)});
		}
		return asset;
	}
};

}// namespace

TEST_F(NavPathfinding, aStraightRoadIsOneStepPerCell) {
	auto asset = makeAsset(16, 4);
	paintRow(asset, 2, 11, 1);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult path;
	ASSERT_TRUE(finder.find(map, {2, 1}, {11, 1}, PathMode::Flat, path));
	ASSERT_EQ(path.cells.size(), 10u);
	EXPECT_EQ(path.cells.front(), (math::vec2ui{2, 1}));
	EXPECT_EQ(path.cells.back(), (math::vec2ui{11, 1}));
	EXPECT_EQ(path.cost, 9u * edgeUnit);
	EXPECT_FLOAT_EQ(path.getCostInCells(), 9.f);
}

TEST_F(NavPathfinding, aCellOffTheGraphHasNoPath) {
	auto asset = makeAsset(8, 4);
	paintRow(asset, 0, 3, 1);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult path;
	EXPECT_FALSE(finder.find(map, {0, 1}, {6, 1}, PathMode::Flat, path));
	EXPECT_FALSE(finder.find(map, {0, 0}, {3, 1}, PathMode::Flat, path));
	EXPECT_FALSE(finder.find(map, {0, 1}, {40, 1}, PathMode::Flat, path));
	EXPECT_EQ(finder.getState(), PathState::NotFound);
}

TEST_F(NavPathfinding, aDisconnectedRoadIsNotFound) {
	auto asset = makeAsset(12, 4);
	paintRow(asset, 0, 3, 1);
	paintRow(asset, 5, 9, 1);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult path;
	EXPECT_FALSE(finder.find(map, {0, 1}, {9, 1}, PathMode::Flat, path));
	EXPECT_FALSE(finder.find(map, {0, 1}, {9, 1}, PathMode::Hierarchical, path));
}

TEST_F(NavPathfinding, aStartOnTheGoalIsAPathOfOneCell) {
	auto asset = makeAsset(8, 4);
	paintRow(asset, 0, 3, 1);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult path;
	for (const auto mode: {PathMode::Flat, PathMode::Hierarchical}) {
		ASSERT_TRUE(finder.find(map, {2, 1}, {2, 1}, mode, path));
		EXPECT_EQ(path.cells.size(), 1u);
		EXPECT_EQ(path.cost, 0u);
	}
}

TEST_F(NavPathfinding, theCheaperRouteWinsOverTheShorterOne) {
	auto asset = makeAsset(12, 6);
	paintRow(asset, 0, 8, 1, slowRoad);// straight but slow: 9 cells at cost 3
	paintColumn(asset, 0, 1, 3);
	paintRow(asset, 0, 8, 3);// a detour of 2 extra cells each side at cost 1
	paintColumn(asset, 8, 1, 3);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult path;
	ASSERT_TRUE(finder.find(map, {0, 1}, {8, 1}, PathMode::Flat, path));
	EXPECT_EQ(path.cost, referenceCost(map, {0, 1}, {8, 1}));
	EXPECT_EQ(path.cells.size(), 13u) << "the detour through the row 3";
	EXPECT_LT(path.cost, 8u * 3u * edgeUnit);
}

TEST_F(NavPathfinding, aOneWayRoadIsDrivenInItsDirectionOnly) {
	auto asset = makeAsset(12, 6);
	paintRow(asset, 1, 8, 1, eastOnly);
	paintRow(asset, 0, 9, 3);
	paintColumn(asset, 0, 1, 3);
	paintColumn(asset, 9, 1, 3);
	paint(asset, 0, 1, road);
	paint(asset, 9, 1, road);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult path;
	ASSERT_TRUE(finder.find(map, {0, 1}, {9, 1}, PathMode::Flat, path));
	EXPECT_EQ(path.cells.size(), 10u) << "east along the one-way row";
	EXPECT_EQ(map.getEdgeCost(4, 1, Direction::West), 0u);
	EXPECT_NE(map.getEdgeCost(4, 1, Direction::East), 0u);
	ASSERT_TRUE(finder.find(map, {9, 1}, {0, 1}, PathMode::Flat, path));
	EXPECT_EQ(path.cells.size(), 14u) << "back by the row 3";
	EXPECT_EQ(path.cost, referenceCost(map, {9, 1}, {0, 1}));
}

TEST_F(NavPathfinding, aOneWayRoadWithoutAnotherWayIsNotFoundBackwards) {
	auto asset = makeAsset(12, 3);
	paintRow(asset, 1, 8, 1, eastOnly);
	paint(asset, 0, 1, road);
	paint(asset, 9, 1, road);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult path;
	EXPECT_TRUE(finder.find(map, {0, 1}, {9, 1}, PathMode::Flat, path));
	EXPECT_FALSE(finder.find(map, {9, 1}, {0, 1}, PathMode::Flat, path));
	EXPECT_FALSE(finder.find(map, {9, 1}, {0, 1}, PathMode::Hierarchical, path));
}

TEST_F(NavPathfinding, enteringRulesKeepTheSideStreetsOut) {
	auto asset = makeAsset(8, 8);
	paintRow(asset, 0, 6, 3);
	paint(asset, 3, 2, road);
	auto profile = makeProfile();
	profile.rules.push_back({.tile = 4, .enters = directionBit(Direction::East) | directionBit(Direction::West)});
	paint(asset, 3, 3, 4);
	const auto map = makeMap(asset, profile);
	EXPECT_EQ(map.getEdgeCost(3, 2, Direction::South), 0u) << "the cell cannot be entered heading south";
	EXPECT_NE(map.getEdgeCost(2, 3, Direction::East), 0u);
}

TEST_F(NavPathfinding, aSteepSlopeBlocksTheCellsOnIt) {
	auto asset = makeAsset(8, 3);
	paintRow(asset, 0, 7, 1);
	std::ignore = asset.setCornerHeight(4, 1, 2);
	EXPECT_TRUE(terrain::isSteepSlope(asset.getCellSlope(3, 0)) || terrain::isSteepSlope(asset.getCellSlope(3, 1)) ||
				terrain::isSteepSlope(asset.getCellSlope(4, 1)));
	auto profile = makeProfile();
	const auto blocked = makeMap(asset, profile);
	Pathfinder finder;
	PathResult path;
	bool anySteep = false;
	for (uint32_t x = 0; x < 8; ++x)
		anySteep = anySteep || (terrain::isSteepSlope(asset.getCellSlope(x, 1)) && !blocked.isWalkable(x, 1));
	EXPECT_TRUE(anySteep) << "a steep cell of the road is not a node";
	EXPECT_FALSE(finder.find(blocked, {0, 1}, {7, 1}, PathMode::Flat, path));
	profile.allowSteep = true;
	const auto open = makeMap(asset, profile);
	EXPECT_TRUE(finder.find(open, {0, 1}, {7, 1}, PathMode::Flat, path));
}

TEST_F(NavPathfinding, noMoveCrossesAMoreThanMaxRiseSlope) {
	auto asset = makeAsset(8, 3);
	paintRow(asset, 0, 7, 1);
	std::ignore = asset.raiseArea({4, 0}, {8, 3});
	std::ignore = asset.raiseArea({4, 0}, {8, 3});
	auto profile = makeProfile();
	profile.allowSteep = true;
	profile.maxRise = 0;
	const auto flatOnly = makeMap(asset, profile);
	Pathfinder finder;
	PathResult path;
	EXPECT_FALSE(finder.find(flatOnly, {0, 1}, {7, 1}, PathMode::Flat, path)) << "the ramp rises on the way";
	profile.maxRise = 1;
	const auto climb = makeMap(asset, profile);
	EXPECT_TRUE(finder.find(climb, {0, 1}, {7, 1}, PathMode::Flat, path));
	EXPECT_EQ(path.cost, referenceCost(climb, {0, 1}, {7, 1}));
}

TEST_F(NavPathfinding, climbingCostsMoreThanTheFlatDetour) {
	auto asset = makeAsset(12, 7);
	paintRow(asset, 0, 10, 1);
	paintRow(asset, 0, 10, 5);
	paintColumn(asset, 0, 1, 5);
	paintColumn(asset, 10, 1, 5);
	std::ignore = asset.raiseArea({4, 0}, {7, 3});
	auto cheap = makeProfile();
	cheap.climbCost = 0.f;
	auto steep = makeProfile();
	steep.climbCost = 20.f;
	Pathfinder finder;
	PathResult direct;
	PathResult detour;
	ASSERT_TRUE(finder.find(makeMap(asset, cheap), {0, 1}, {10, 1}, PathMode::Flat, direct));
	const auto expensive = makeMap(asset, steep);
	ASSERT_TRUE(finder.find(expensive, {0, 1}, {10, 1}, PathMode::Flat, detour));
	EXPECT_EQ(direct.cells.size(), 11u);
	EXPECT_GT(detour.cells.size(), direct.cells.size()) << "a heavy climb sends the path round the hill";
	EXPECT_EQ(detour.cost, referenceCost(expensive, {0, 1}, {10, 1}));
	EXPECT_EQ(pathCost(expensive, detour), detour.cost);
}

TEST_F(NavPathfinding, theTerrainLayerCanBeTheGraph) {
	auto asset = makeAsset(6, 6);
	asset.layers[0].fill(5);
	asset.layers[0].setTile(2, 2, 9);
	NavProfile profile;
	profile.source = NavSource::Terrain;
	profile.rules.push_back({.tile = 5});
	const auto map = makeMap(asset, profile);
	EXPECT_FALSE(map.isWalkable(2, 2));
	Pathfinder finder;
	PathResult path;
	ASSERT_TRUE(finder.find(map, {0, 2}, {5, 2}, PathMode::Flat, path));
	EXPECT_EQ(path.cost, referenceCost(map, {0, 2}, {5, 2}));
	EXPECT_EQ(path.cells.size(), 8u) << "round the blocked cell";
}

TEST_F(NavPathfinding, flatMatchesAnIndependentDijkstraOnRandomMaps) {
	Pathfinder finder;
	PathResult path;
	for (uint32_t seed = 1; seed <= 12; ++seed) {
		const auto asset = makeRandomAsset(40, 30, seed);
		auto profile = makeProfile();
		profile.allowSteep = seed % 2 == 0;
		profile.maxRise = static_cast<uint8_t>(seed % 3);
		const auto map = makeMap(asset, profile);
		std::mt19937 random{seed * 977};
		for (int query = 0; query < 25; ++query) {
			const math::vec2ui from{roll(random, 40), roll(random, 30)};
			const math::vec2ui to{roll(random, 40), roll(random, 30)};
			const auto expected = referenceCost(map, from, to);
			const bool found = finder.find(map, from, to, PathMode::Flat, path);
			if (!map.isWalkable(from.x(), from.y()) || !map.isWalkable(to.x(), to.y())) {
				EXPECT_FALSE(found);
				continue;
			}
			ASSERT_EQ(found, expected != unreachable) << "seed " << seed << " query " << query;
			if (found) {
				EXPECT_EQ(path.cost, expected);
				EXPECT_EQ(pathCost(map, path), expected);
				EXPECT_TRUE(isPathValid(map, path.cells));
			}
		}
	}
}

TEST_F(NavPathfinding, hierarchicalCostsTheSameAsFlatOnRandomMaps) {
	Pathfinder finder;
	PathResult flat;
	PathResult hierarchical;
	uint32_t compared = 0;
	for (uint32_t seed = 1; seed <= 8; ++seed) {
		const auto asset = makeRandomAsset(110, 75, seed + 100);
		auto profile = makeProfile();
		profile.maxRise = 1;
		profile.allowSteep = seed % 2 == 1;
		const auto map = makeMap(asset, profile);
		std::mt19937 random{seed * 31};
		for (int query = 0; query < 25; ++query) {
			const math::vec2ui from{roll(random, 110), roll(random, 75)};
			const math::vec2ui to{roll(random, 110), roll(random, 75)};
			const bool foundFlat = finder.find(map, from, to, PathMode::Flat, flat);
			const bool foundHier = finder.find(map, from, to, PathMode::Hierarchical, hierarchical);
			ASSERT_EQ(foundFlat, foundHier) << "seed " << seed << " query " << query;
			if (!foundFlat)
				continue;
			++compared;
			EXPECT_EQ(hierarchical.cost, flat.cost) << "seed " << seed << " query " << query;
			EXPECT_EQ(pathCost(map, hierarchical), hierarchical.cost);
			EXPECT_EQ(hierarchical.cells.front(), from);
			EXPECT_EQ(hierarchical.cells.back(), to);
		}
	}
	EXPECT_GT(compared, 40u) << "the random maps join enough cells";
}

TEST_F(NavPathfinding, hierarchicalFindsAPathThatLeavesTheChunkAndComesBack) {
	auto asset = makeAsset(70, 40);
	paintColumn(asset, 5, 2, 20);
	paintRow(asset, 5, 40, 20);
	paintColumn(asset, 40, 2, 20);
	paintRow(asset, 5, 40, 2);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult flat;
	PathResult hierarchical;
	ASSERT_TRUE(finder.find(map, {5, 2}, {40, 2}, PathMode::Flat, flat));
	ASSERT_TRUE(finder.find(map, {5, 2}, {40, 2}, PathMode::Hierarchical, hierarchical));
	EXPECT_EQ(hierarchical.cost, flat.cost);
	ASSERT_TRUE(finder.find(map, {5, 20}, {40, 20}, PathMode::Hierarchical, hierarchical));
	ASSERT_TRUE(finder.find(map, {5, 20}, {40, 20}, PathMode::Flat, flat));
	EXPECT_EQ(hierarchical.cost, flat.cost);
	EXPECT_EQ(hierarchical.cells.size(), flat.cells.size());
}

TEST_F(NavPathfinding, autoModePicksTheSameCostAsFlat) {
	const auto asset = makeRandomAsset(100, 40, 7);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult flat;
	PathResult automatic;
	auto random = makeRandom(5);
	for (int query = 0; query < 30; ++query) {
		const math::vec2ui from{roll(random, 100), roll(random, 40)};
		const math::vec2ui to{roll(random, 100), roll(random, 40)};
		const bool foundFlat = finder.find(map, from, to, PathMode::Flat, flat);
		ASSERT_EQ(foundFlat, finder.find(map, from, to, PathMode::Auto, automatic));
		if (foundFlat) {
			EXPECT_EQ(automatic.cost, flat.cost);
		}
	}
}

TEST_F(NavPathfinding, anEditRebuildsOnlyItsChunkAndChangesThePath) {
	auto asset = makeAsset(100, 70);
	paintRow(asset, 0, 95, 5);
	paintRow(asset, 0, 95, 9);
	paintColumn(asset, 0, 5, 9);
	paintColumn(asset, 95, 5, 9);
	auto map = makeMap(asset, makeProfile());
	Pathfinder finder;
	PathResult before;
	ASSERT_TRUE(finder.find(map, {0, 5}, {95, 5}, PathMode::Hierarchical, before));
	EXPECT_EQ(before.cells.size(), 96u);
	const auto rebuiltBefore = map.getRebuiltChunkCount();
	const auto versionBefore = map.getVersion();

	EXPECT_EQ(map.sync(asset), 0u) << "nothing changed";
	EXPECT_EQ(map.getVersion(), versionBefore);

	paint(asset, 50, 5, -1);
	EXPECT_EQ(map.sync(asset), 1u);
	EXPECT_EQ(map.getRebuiltChunkCount(), rebuiltBefore + 1);
	EXPECT_NE(map.getVersion(), versionBefore);
	EXPECT_FALSE(map.isWalkable(50, 5));

	PathResult after;
	ASSERT_TRUE(finder.find(map, {0, 5}, {95, 5}, PathMode::Hierarchical, after));
	EXPECT_GT(after.cost, before.cost);
	EXPECT_EQ(after.cells.size(), 96u + 8u) << "round by the row 9";
	PathResult flat;
	ASSERT_TRUE(finder.find(map, {0, 5}, {95, 5}, PathMode::Flat, flat));
	EXPECT_EQ(after.cost, flat.cost);
	EXPECT_FALSE(isPathValid(map, before.cells)) << "the old path crosses the bulldozed cell";
	EXPECT_TRUE(isPathValid(map, after.cells));

	paint(asset, 50, 5, road);
	EXPECT_EQ(map.sync(asset), 1u);
	ASSERT_TRUE(finder.find(map, {0, 5}, {95, 5}, PathMode::Hierarchical, after));
	EXPECT_EQ(after.cost, before.cost) << "the road is back";
}

TEST_F(NavPathfinding, anIncrementalMapMatchesAFreshOneAfterManyEdits) {
	auto asset = makeRandomAsset(100, 70, 11);
	const auto profile = makeProfile();
	auto incremental = makeMap(asset, profile);
	auto random = makeRandom(3);
	Pathfinder finder;
	for (int round = 0; round < 6; ++round) {
		for (int edit = 0; edit < 40; ++edit)
			paint(asset, roll(random, 100), roll(random, 70),
				  static_cast<int32_t>(roll(random, 4)) - (roll(random, 4) == 0 ? 2 : 0));
		if (round % 2 == 1)
			std::ignore = asset.raiseArea({roll(random, 90), roll(random, 60)}, {roll(random, 100), roll(random, 70)});
		incremental.sync(asset);
		const auto fresh = makeMap(asset, profile);
		PathResult left;
		PathResult right;
		for (int query = 0; query < 12; ++query) {
			const math::vec2ui from{roll(random, 100), roll(random, 70)};
			const math::vec2ui to{roll(random, 100), roll(random, 70)};
			const bool foundLeft = finder.find(incremental, from, to, PathMode::Hierarchical, left);
			ASSERT_EQ(foundLeft, finder.find(fresh, from, to, PathMode::Flat, right)) << "round " << round;
			if (foundLeft) {
				EXPECT_EQ(left.cost, right.cost) << "round " << round << " query " << query;
			}
		}
	}
}

TEST_F(NavPathfinding, aHeightChangeRebuildsTheChunksAroundIt) {
	auto asset = makeAsset(70, 40);
	paintRow(asset, 0, 60, 5);
	auto map = makeMap(asset, makeProfile());
	EXPECT_EQ(map.sync(asset), 0u);
	std::ignore = asset.raiseCorner(33, 5);
	EXPECT_GE(map.sync(asset), 1u);
	std::ignore = map.sync(asset);
	EXPECT_EQ(map.sync(asset), 0u);
}

TEST_F(NavPathfinding, newRulesRebuildEverything) {
	auto asset = makeAsset(40, 40);
	paintRow(asset, 0, 30, 3);
	auto map = makeMap(asset, makeProfile());
	auto profile = makeProfile();
	profile.rules.clear();
	profile.rules.push_back({.tile = road, .cost = 2.f});
	map.setProfile(profile);
	EXPECT_GE(map.sync(asset), 1u);
	EXPECT_FLOAT_EQ(map.getCellCost(4, 3), 2.f);
	EXPECT_EQ(map.getMinEdgeCost(), 4u * 16u / 2u * 2u);
}

TEST_F(NavPathfinding, aSearchCutInSlicesEndsWithTheWholeSearchResult) {
	const auto asset = makeRandomAsset(110, 75, 21);
	const auto map = makeMap(asset, makeProfile());
	Pathfinder whole;
	Pathfinder sliced;
	PathResult expected;
	PathResult got;
	auto random = makeRandom(9);
	int checked = 0;
	for (int query = 0; query < 20; ++query) {
		const math::vec2ui from{roll(random, 110), roll(random, 75)};
		const math::vec2ui to{roll(random, 110), roll(random, 75)};
		for (const auto mode: {PathMode::Flat, PathMode::Hierarchical}) {
			const bool found = whole.find(map, from, to, mode, expected);
			sliced.begin(map, from, to, mode);
			int slices = 0;
			while (sliced.getState() == PathState::Searching && slices < 100000) {
				sliced.step(50);
				++slices;
			}
			ASSERT_EQ(sliced.takeResult(got), found);
			if (found) {
				EXPECT_EQ(got.cost, expected.cost);
				++checked;
			}
		}
	}
	EXPECT_GT(checked, 10);
}

TEST_F(NavPathfinding, theQueueSpreadsALongQueryOverSeveralUpdates) {
	auto asset = makeAsset(200, 60);
	paintRow(asset, 0, 190, 10);
	const auto map = makeMap(asset, makeProfile());
	PathQueue queue;
	const auto request = queue.request({0, 10}, {190, 10}, PathMode::Flat);
	EXPECT_EQ(queue.getStatus(request), PathStatus::Pending);
	EXPECT_EQ(queue.getResult(request), nullptr);
	int updates = 0;
	while (queue.getStatus(request) == PathStatus::Pending && updates < 1000) {
		std::ignore = queue.update(map, 40);
		++updates;
	}
	EXPECT_GT(updates, 2) << "a budget of 40 expansions cannot cross 190 cells at once";
	ASSERT_EQ(queue.getStatus(request), PathStatus::Found);
	ASSERT_NE(queue.getResult(request), nullptr);
	EXPECT_EQ(queue.getResult(request)->cells.size(), 191u);
	queue.release(request);
	EXPECT_EQ(queue.getStatus(request), PathStatus::Unknown);
	EXPECT_EQ(queue.getPendingCount(), 0u);
}

TEST_F(NavPathfinding, theQueueServesRequestsInOrderAndReportsFailures) {
	auto asset = makeAsset(40, 10);
	paintRow(asset, 0, 10, 2);
	const auto map = makeMap(asset, makeProfile());
	PathQueue queue;
	const auto first = queue.request({0, 2}, {10, 2});
	const auto second = queue.request({0, 2}, {30, 2});
	const auto third = queue.request({0, 0}, {10, 2});
	EXPECT_EQ(queue.getPendingCount(), 3u);
	std::ignore = queue.update(map, 100000);
	EXPECT_EQ(queue.getStatus(first), PathStatus::Found);
	EXPECT_EQ(queue.getStatus(second), PathStatus::NotFound);
	EXPECT_EQ(queue.getStatus(third), PathStatus::NotFound);
	EXPECT_EQ(queue.getPendingCount(), 0u);
	const auto reused = queue.request({0, 2}, {5, 2});
	EXPECT_NE(reused, first);
	queue.clear();
	EXPECT_EQ(queue.getStatus(reused), PathStatus::Unknown);
}

TEST_F(NavPathfinding, aMapChangeRestartsTheSearchInProgress) {
	auto asset = makeAsset(120, 30);
	paintRow(asset, 0, 110, 4);
	paintRow(asset, 0, 110, 8);
	paintColumn(asset, 0, 4, 8);
	paintColumn(asset, 110, 4, 8);
	auto map = makeMap(asset, makeProfile());
	PathQueue queue;
	const auto request = queue.request({0, 4}, {110, 4}, PathMode::Flat);
	std::ignore = queue.update(map, 20);
	ASSERT_EQ(queue.getStatus(request), PathStatus::Pending);
	paint(asset, 60, 4, -1);
	map.sync(asset);
	std::ignore = queue.update(map, 1000000);
	ASSERT_EQ(queue.getStatus(request), PathStatus::Found);
	EXPECT_EQ(queue.getResult(request)->cells.size(), 111u + 8u);
}

TEST_F(NavPathfinding, aSteadyQueueReusesItsBuffers) {
	auto asset = makeAsset(100, 40);
	paintRow(asset, 0, 90, 4);
	const auto map = makeMap(asset, makeProfile());
	PathQueue queue;
	auto id = queue.request({0, 4}, {90, 4});
	std::ignore = queue.update(map, 1000000);
	ASSERT_NE(queue.getResult(id), nullptr);
	queue.release(id);
	size_t capacity = 0;
	for (int round = 0; round < 4; ++round) {
		id = queue.request({0, 4}, {90, 4});
		std::ignore = queue.update(map, 1000000);
		const auto* result = queue.getResult(id);
		ASSERT_NE(result, nullptr);
		if (round >= 2) {
			EXPECT_EQ(result->cells.capacity(), capacity) << "the path buffers settle after two rounds";
		}
		capacity = result->cells.capacity();
		queue.release(id);
	}
}

TEST_F(NavPathfinding, theTilemapComponentKeepsItsNavigationRules) {
	auto scene = mkShared<Scene>();
	auto& tilemap = scene->createEntityWithUUID(0xABCD, "Map").addComponent<component::Tilemap>();
	tilemap.tilemapPath = "tilemaps/town.owltilemap";
	tilemap.navigation = makeProfile();
	tilemap.navigation.layer = 2;
	tilemap.navigation.maxRise = 2;
	tilemap.navigation.climbCost = 1.25f;
	tilemap.navigation.allowSteep = true;
	tilemap.navigation.rules.push_back({.tile = 7, .cost = 0.5f, .exits = 0, .enters = directionBit(Direction::North)});
	const auto yaml = SceneSerializer{scene}.serializeToString();
	EXPECT_NE(yaml.find("navigation"), std::string::npos);
	auto loaded = mkShared<Scene>();
	const std::vector<uint8_t> bytes(yaml.begin(), yaml.end());
	ASSERT_TRUE(SceneSerializer{loaded}.deserializeFromBuffer(bytes));
	const auto entities = loaded->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	EXPECT_EQ(entities[0].getComponent<component::Tilemap>().navigation, tilemap.navigation);
}

TEST_F(NavPathfinding, aTilemapWithoutRulesWritesNoNavigation) {
	auto scene = mkShared<Scene>();
	scene->createEntity("Map").addComponent<component::Tilemap>().tilemapPath = "tilemaps/town.owltilemap";
	EXPECT_EQ(SceneSerializer{scene}.serializeToString().find("navigation"), std::string::npos);
}

TEST_F(NavPathfinding, theSceneSystemServesTheRequestsOfItsTilemaps) {
	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Map");
	auto asset = mkShared<TilemapAsset>(makeAsset(120, 20));
	paintRow(*asset, 0, 100, 3);
	auto& tilemap = entity.addComponent<component::Tilemap>();
	tilemap.asset = asset;
	tilemap.navigation = makeProfile();
	auto& world = scene->getNavigation();
	world.setBudget(30);
	auto& entry = world.acquire(static_cast<uint64_t>(entity.getUUID()), *asset, tilemap.navigation);
	const auto request = entry.queue.request({0, 3}, {100, 3}, PathMode::Flat);
	int frames = 0;
	while (entry.queue.getStatus(request) == PathStatus::Pending && frames < 1000) {
		world.update(*scene);
		++frames;
	}
	EXPECT_GT(frames, 2);
	EXPECT_EQ(entry.queue.getStatus(request), PathStatus::Found);
	EXPECT_EQ(world.getBudget(), 30u);
	EXPECT_EQ(world.find(1234567), nullptr);
	world.clear();
	EXPECT_EQ(world.find(static_cast<uint64_t>(entity.getUUID())), nullptr);
}
