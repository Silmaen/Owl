/**
 * @file NavConnections_test.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>
#include <scene/component/Tilemap.h>
#include <scene/nav/NavMap.h>
#include <scene/nav/NavProfile.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::scene;
using namespace owl::scene::nav;

namespace {

constexpr uint8_t E = 1u << 0u;
constexpr uint8_t S = 1u << 1u;
constexpr uint8_t W = 1u << 2u;
constexpr uint8_t N = 1u << 3u;

/// One road shape of iso_town: its tile, the sides it joins and the old hand-written rule (exits / enters).
struct RoadShape {
	int32_t tile;
	uint8_t joins;
	uint8_t enters;
};

/// The 16 hand-written rules iso_town used before the tileset carried the connections.
constexpr std::array<RoadShape, 16> g_oldRules{{
		{3, E | W, E | W},
		{11, S | N, S | N},
		{12, 0, 0},
		{13, N, S},
		{14, E, W},
		{15, S, N},
		{16, W, E},
		{17, E | N, S | W},
		{18, E | S, W | N},
		{19, S | W, E | N},
		{20, W | N, E | S},
		{21, E | S | N, S | W | N},
		{22, E | S | W, E | W | N},
		{23, S | W | N, E | S | N},
		{24, E | W | N, E | S | W},
		{25, E | S | W | N, E | S | W | N},
}};

class NavConnections : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }

	static void TearDownTestSuite() { core::Log::invalidate(); }

	static auto makeTileset() -> shared<Tileset> {
		auto tileset = mkShared<Tileset>();
		tileset->resize(8, 8);
		for (const auto& shape: g_oldRules) tileset->tiles[static_cast<size_t>(shape.tile)].connections = shape.joins;
		return tileset;
	}

	static auto makeAsset(const uint32_t iSize) -> shared<TilemapAsset> {
		auto asset = mkShared<TilemapAsset>();
		asset->resize(iSize, iSize);
		asset->addLayer("roads");
		asset->tileset = makeTileset();
		return asset;
	}

	static void paint(TilemapAsset& ioAsset, const uint32_t iX, const uint32_t iY, const int32_t iTile) {
		auto cell = ioAsset.layers[0].getCell(iX, iY);
		cell.overlay = iTile;
		std::ignore = ioAsset.layers[0].setCell(iX, iY, cell);
	}

	static auto connectionsProfile() -> NavProfile {
		NavProfile profile;
		profile.mode = NavMode::Connections;
		return profile;
	}

	static auto makeMap(const TilemapAsset& iAsset, const NavProfile& iProfile) -> NavMap {
		NavMap map;
		map.setProfile(iProfile);
		map.sync(iAsset);
		return map;
	}

	/// A straight road along x from 0 to 5 on row 2.
	static auto makeStreet() -> shared<TilemapAsset> {
		auto asset = makeAsset(8);
		for (uint32_t x = 0; x <= 5; ++x) paint(*asset, x, 2, 3);
		return asset;
	}
};

}// namespace

TEST_F(NavConnections, tilesetConnectionsRoundTripThroughTheFile) {
	auto tileset = makeTileset();
	tileset->tiles[40].connections = S;
	const auto yaml = tileset->serializeToString("roads");
	EXPECT_NE(yaml.find("connections: [east, west]"), std::string::npos);
	EXPECT_NE(yaml.find("connections: []"), std::string::npos) << "an isolated road tile keeps its empty mask";
	Tileset loaded;
	ASSERT_TRUE(loaded.deserializeFromString(yaml));
	ASSERT_EQ(loaded.tiles.size(), tileset->tiles.size());
	for (size_t i = 0; i < loaded.tiles.size(); ++i)
		EXPECT_EQ(loaded.tiles[i].connections, tileset->tiles[i].connections) << i;
	EXPECT_FALSE(loaded.tiles[0].connections.has_value());
	EXPECT_EQ(loaded.tiles[12].connections, std::optional<uint8_t>{0});
	EXPECT_EQ(loaded.tiles[40].connections, std::optional<uint8_t>{S});
}

TEST_F(NavConnections, aTilesetWithoutConnectionsStillLoads) {
	Tileset legacy;
	ASSERT_TRUE(legacy.deserializeFromString(
			"Tileset: old\nFormatVersion: 1\ntileWidth: 32\ntileHeight: 32\ncolumns: 2\nrows: 2\n"
			"tiles:\n  - {index: 1, collidable: true, name: wall}\n"));
	EXPECT_TRUE(legacy.tiles[1].collidable);
	for (const auto& meta: legacy.tiles) EXPECT_FALSE(meta.connections.has_value());
	EXPECT_EQ(legacy.serializeToString().find("connections"), std::string::npos);
}

TEST_F(NavConnections, theGraphFromTheConnectionsIsTheOneOfTheOldRules) {
	NavProfile rules;
	rules.mode = NavMode::Rules;
	for (const auto& shape: g_oldRules)
		rules.rules.push_back({.tile = shape.tile, .exits = shape.joins, .enters = shape.enters});
	constexpr uint32_t size = 24;
	auto asset = makeAsset(size);
	std::mt19937 random{7};// NOLINT(bugprone-random-generator-seed): a fixed seed keeps the random maps reproducible
	for (uint32_t y = 0; y < size; ++y) {
		for (uint32_t x = 0; x < size; ++x) {
			if (random() % 3 != 0)
				paint(*asset, x, y, g_oldRules[random() % g_oldRules.size()].tile);
		}
	}
	const auto fromRules = makeMap(*asset, rules);
	const auto fromConnections = makeMap(*asset, connectionsProfile());
	EXPECT_GT(fromRules.getWalkableCount(), 100u);
	EXPECT_EQ(fromConnections.getWalkableCount(), fromRules.getWalkableCount());
	for (uint32_t y = 0; y < size; ++y) {
		for (uint32_t x = 0; x < size; ++x) {
			EXPECT_EQ(fromConnections.isWalkable(x, y), fromRules.isWalkable(x, y));
			for (const auto direction: directions)
				EXPECT_EQ(fromConnections.getEdgeCost(x, y, direction), fromRules.getEdgeCost(x, y, direction))
						<< x << "," << y;
		}
	}
}

TEST_F(NavConnections, editingTheTilesetRebuildsTheGraph) {
	const auto asset = makeStreet();
	NavMap map;
	map.setProfile(connectionsProfile());
	map.sync(*asset);
	EXPECT_NE(map.getEdgeCost(1, 2, Direction::East), 0u);
	asset->tileset->tiles[3].connections = N;
	EXPECT_GT(map.sync(*asset), 0u);
	EXPECT_EQ(map.getEdgeCost(1, 2, Direction::East), 0u);
	asset->tileset->tiles[3].connections.reset();
	map.sync(*asset);
	EXPECT_EQ(map.getWalkableCount(), 0u);
}

TEST_F(NavConnections, anyCellModeWalksOnEveryNonEmptyCell) {
	auto asset = makeAsset(6);
	paint(*asset, 1, 1, 30);
	paint(*asset, 2, 1, 31);
	NavProfile profile;
	profile.mode = NavMode::AnyCell;
	const auto map = makeMap(*asset, profile);
	EXPECT_EQ(map.getWalkableCount(), 2u);
	EXPECT_NE(map.getEdgeCost(1, 1, Direction::East), 0u);
	EXPECT_NE(map.getEdgeCost(2, 1, Direction::West), 0u);
	EXPECT_FALSE(profile.isEmpty());
}

TEST_F(NavConnections, aTilemapWithNoTilesetHasNoRoads) {
	auto asset = makeStreet();
	asset->tileset.reset();
	EXPECT_EQ(makeMap(*asset, connectionsProfile()).getWalkableCount(), 0u);
}

TEST_F(NavConnections, theBaseCellTellsWhatTheTileOpens) {
	const auto asset = makeStreet();
	const auto map = makeMap(*asset, connectionsProfile());
	const auto base = map.getBaseCell(*asset, 2, 2);
	ASSERT_TRUE(base.has_value());
	EXPECT_EQ(base->exits, E | W);
	EXPECT_EQ(base->enters, E | W);
	EXPECT_FALSE(map.getBaseCell(*asset, 2, 3).has_value());
	EXPECT_FALSE(map.getBaseCell(*asset, 99, 3).has_value());
}

TEST_F(NavConnections, anEdgeCyclesThroughTheFourStates) {
	EXPECT_EQ(nextSideState(SideState::TwoWay), SideState::ExitOnly);
	EXPECT_EQ(nextSideState(SideState::ExitOnly), SideState::EnterOnly);
	EXPECT_EQ(nextSideState(SideState::EnterOnly), SideState::Closed);
	EXPECT_EQ(nextSideState(SideState::Closed), SideState::TwoWay);
}

TEST_F(NavConnections, anExitOnlySideBlocksTheWayBackOnBothCells) {
	const auto asset = makeStreet();
	auto profile = connectionsProfile();
	profile.setEdgeState(2, 2, Direction::East, SideState::ExitOnly);
	EXPECT_EQ(profile.getSideState(2, 2, Direction::East), SideState::ExitOnly);
	EXPECT_EQ(profile.getSideState(3, 2, Direction::West), SideState::EnterOnly) << "the neighbour agrees";
	const auto map = makeMap(*asset, profile);
	EXPECT_NE(map.getEdgeCost(2, 2, Direction::East), 0u);
	EXPECT_EQ(map.getEdgeCost(3, 2, Direction::West), 0u);
	EXPECT_NE(map.getEdgeCost(3, 2, Direction::East), 0u) << "the rest of the street stays two-way";
	profile.setEdgeState(2, 2, Direction::East, SideState::EnterOnly);
	const auto reversed = makeMap(*asset, profile);
	EXPECT_EQ(reversed.getEdgeCost(2, 2, Direction::East), 0u);
	EXPECT_NE(reversed.getEdgeCost(3, 2, Direction::West), 0u);
}

TEST_F(NavConnections, aClosedSideCutsTheStreet) {
	const auto asset = makeStreet();
	auto profile = connectionsProfile();
	profile.setEdgeState(2, 2, Direction::East, SideState::Closed);
	const auto map = makeMap(*asset, profile);
	EXPECT_EQ(map.getEdgeCost(2, 2, Direction::East), 0u);
	EXPECT_EQ(map.getEdgeCost(3, 2, Direction::West), 0u);
	EXPECT_NE(map.getEdgeCost(1, 2, Direction::East), 0u);
	profile.setEdgeState(2, 2, Direction::East, SideState::TwoWay);
	EXPECT_TRUE(profile.overrides.empty()) << "a two-way side leaves no exception behind";
}

TEST_F(NavConnections, anExceptionNeverOpensWhatTheTileKeepsClosed) {
	const auto asset = makeStreet();
	auto profile = connectionsProfile();
	profile.setEdgeState(2, 2, Direction::South, SideState::TwoWay);
	profile.setEdgeState(2, 2, Direction::North, SideState::ExitOnly);
	const auto map = makeMap(*asset, profile);
	EXPECT_EQ(map.getEdgeCost(2, 2, Direction::North), 0u);
	EXPECT_EQ(map.getEdgeCost(2, 2, Direction::South), 0u);
}

TEST_F(NavConnections, aCellCostMakesTheCellSlower) {
	const auto asset = makeStreet();
	auto profile = connectionsProfile();
	const auto plain = makeMap(*asset, profile).getEdgeCost(2, 2, Direction::East);
	profile.setCellCost(3, 2, 3.f);
	const auto map = makeMap(*asset, profile);
	EXPECT_FLOAT_EQ(map.getCellCost(3, 2), 3.f);
	EXPECT_GT(map.getEdgeCost(2, 2, Direction::East), plain);
	EXPECT_EQ(map.getEdgeCost(2, 2, Direction::East), plain + 2u * 16u);
	profile.setCellCost(3, 2, 0.f);
	EXPECT_TRUE(profile.overrides.empty());
}

TEST_F(NavConnections, theCheaperCellKeepsTheSearchHeuristicAdmissible) {
	auto profile = connectionsProfile();
	profile.setCellCost(3, 2, 0.25f);
	const auto asset = makeStreet();
	EXPECT_LE(makeMap(*asset, profile).getMinEdgeCost(), 8u);
}

TEST_F(NavConnections, theNavigationOfTheComponentRoundTripsWithItsExceptions) {
	auto scene = mkShared<Scene>();
	auto& tilemap = scene->createEntityWithUUID(0xABCD, "Map").addComponent<component::Tilemap>();
	tilemap.tilemapPath = "tilemaps/town.owltilemap";
	tilemap.navigation = connectionsProfile();
	tilemap.navigation.layer = 1;
	tilemap.navigation.setEdgeState(4, 5, Direction::East, SideState::ExitOnly);
	tilemap.navigation.setEdgeState(7, 7, Direction::North, SideState::Closed);
	tilemap.navigation.setCellCost(4, 5, 2.5f);
	const auto yaml = SceneSerializer{scene}.serializeToString();
	EXPECT_NE(yaml.find("mode: connections"), std::string::npos);
	EXPECT_EQ(yaml.find("rules"), std::string::npos);
	auto loaded = mkShared<Scene>();
	const std::vector<uint8_t> bytes(yaml.begin(), yaml.end());
	ASSERT_TRUE(SceneSerializer{loaded}.deserializeFromBuffer(bytes));
	const auto entities = loaded->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	EXPECT_EQ(entities[0].getComponent<component::Tilemap>().navigation, tilemap.navigation);
}

TEST_F(NavConnections, aLegacyProfileWithRulesAndNoModeStillLoads) {
	auto scene = mkShared<Scene>();
	auto& tilemap = scene->createEntityWithUUID(0xABCD, "Map").addComponent<component::Tilemap>();
	tilemap.tilemapPath = "tilemaps/town.owltilemap";
	tilemap.navigation.rules.push_back({.tile = 3, .cost = 1.f, .exits = E | W, .enters = E | W});
	tilemap.navigation.rules.push_back({.tile = 13, .cost = 2.f, .exits = N, .enters = S});
	auto yaml = SceneSerializer{scene}.serializeToString();
	// What a file written before the modes looked like: no `mode` key.
	const auto key = yaml.find("mode: rules\n");
	ASSERT_NE(key, std::string::npos);
	const auto lineStart = yaml.rfind('\n', key) + 1;
	yaml.erase(lineStart, key + std::string{"mode: rules\n"}.size() - lineStart);
	EXPECT_EQ(yaml.find("mode:"), std::string::npos) << yaml;
	auto loaded = mkShared<Scene>();
	const std::vector<uint8_t> bytes(yaml.begin(), yaml.end());
	ASSERT_TRUE(SceneSerializer{loaded}.deserializeFromBuffer(bytes)) << yaml;
	const auto entities = loaded->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& navigation = entities[0].getComponent<component::Tilemap>().navigation;
	EXPECT_EQ(navigation.mode, NavMode::Rules);
	EXPECT_EQ(navigation, tilemap.navigation);
	EXPECT_FALSE(navigation.isEmpty());
	EXPECT_TRUE(navigation.overrides.empty());
	const auto asset = makeStreet();
	EXPECT_NE(makeMap(*asset, navigation).getEdgeCost(1, 2, Direction::East), 0u)
			<< "the old rules still build the graph";
}

TEST_F(NavConnections, aProfileOfRulesModeWithoutRulesBuildsNoGraph) {
	NavProfile profile;
	EXPECT_TRUE(profile.isEmpty());
	profile.mode = NavMode::Connections;
	EXPECT_FALSE(profile.isEmpty());
}
