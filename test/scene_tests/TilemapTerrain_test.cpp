/**
 * @file TilemapTerrain_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <renderer/RendererIsometric.h>
#include <scene/TilemapAsset.h>
#include <scene/TilemapCodec.h>
#include <scene/TilemapTerrain.h>
#include <scene/Tileset.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::scene;
using scene::terrain::CornerSet;

namespace {

class TilemapTerrain : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

auto makeMap(const uint32_t iWidth, const uint32_t iHeight) -> TilemapAsset {
	TilemapAsset map;
	map.width = iWidth;
	map.height = iHeight;
	map.addLayer("ground").fill(1);
	return map;
}

// Every corner differs by at most one step from the four next to it.
auto isValid(const TilemapAsset& iMap) -> bool {
	for (uint32_t y = 0; y <= iMap.height; ++y) {
		for (uint32_t x = 0; x <= iMap.width; ++x) {
			const int height = iMap.getCornerHeight(x, y);
			if (std::abs(height - int{iMap.getCornerHeight(x + 1, y)}) > 1 && x < iMap.width)
				return false;
			if (std::abs(height - int{iMap.getCornerHeight(x, y + 1)}) > 1 && y < iMap.height)
				return false;
		}
	}
	return true;
}

auto maxOf(const TilemapAsset& iMap) -> uint8_t {
	uint8_t highest = 0;
	for (uint32_t y = 0; y <= iMap.height; ++y) {
		for (uint32_t x = 0; x <= iMap.width; ++x) highest = std::max(highest, iMap.getCornerHeight(x, y));
	}
	return highest;
}

auto snapshot(const TilemapAsset& iMap) -> std::vector<uint8_t> {
	std::vector<uint8_t> heights;
	for (uint32_t y = 0; y <= iMap.height; ++y) {
		for (uint32_t x = 0; x <= iMap.width; ++x) heights.push_back(iMap.getCornerHeight(x, y));
	}
	return heights;
}

// Corner heights of a 3x3 map whose centre cell has the profile of a slope, raised by a base height.
auto makeSlopeMap(const uint8_t iSlope, const uint8_t iBase) -> TilemapAsset {
	auto map = makeMap(3, 3);
	std::vector<uint8_t> heights(16, 0);
	const auto& profile = terrain::getSlopeProfile(iSlope);
	heights[1 * 4 + 1] = static_cast<uint8_t>(iBase + profile[0]);
	heights[1 * 4 + 2] = static_cast<uint8_t>(iBase + profile[1]);
	heights[2 * 4 + 2] = static_cast<uint8_t>(iBase + profile[2]);
	heights[2 * 4 + 1] = static_cast<uint8_t>(iBase + profile[3]);
	EXPECT_TRUE(map.setCornerHeights(heights));
	return map;
}

// World height of the sloped surface of a cell at a position inside it, split as the picking does.
auto surfaceHeight(const TilemapAsset& iMap, const uint32_t iX, const uint32_t iY, const float iU, const float iV)
		-> float {
	const auto c = iMap.getCellCorners(iX, iY);
	const auto z = [&c](const size_t iIndex) -> float { return static_cast<float>(c[iIndex]) * terrain::heightStep; };
	if (std::abs(int{c[0]} - int{c[2]}) <= std::abs(int{c[1]} - int{c[3]})) {
		// Triangles (back, right, front) and (back, front, left).
		return iU >= iV ? z(0) + (z(1) - z(0)) * (iU - iV) + (z(2) - z(0)) * iV
						: z(0) + (z(2) - z(0)) * iU + (z(3) - z(0)) * (iV - iU);
	}
	// Triangles (back, right, left) and (right, front, left).
	return iU + iV <= 1.f ? z(0) + (z(1) - z(0)) * iU + (z(3) - z(0)) * iV
						  : z(2) + (z(3) - z(2)) * (1.f - iU) + (z(1) - z(2)) * (1.f - iV);
}

}// namespace

TEST_F(TilemapTerrain, EverySlopeIsDerivedFromItsProfile) {
	std::set<std::string_view> names;
	for (uint8_t slope = 0; slope < terrain::slopeCount; ++slope) {
		const auto& profile = terrain::getSlopeProfile(slope);
		const auto derived = terrain::deriveSlope(profile);
		ASSERT_TRUE(derived.has_value()) << int{slope};
		EXPECT_EQ(*derived, slope);
		CornerSet raised{};
		for (size_t i = 0; i < 4; ++i) raised[i] = static_cast<uint8_t>(profile[i] + 7);
		EXPECT_EQ(terrain::deriveSlope(raised), slope) << "the base height does not matter";
		EXPECT_EQ(terrain::isSteepSlope(slope), slope >= 15);
		EXPECT_TRUE(names.insert(terrain::getSlopeName(slope)).second) << "names are unique";
	}
	EXPECT_EQ(terrain::getSlopeName(terrain::slopeCount), "Invalid");
	EXPECT_EQ(names.size(), 19u);
}

TEST_F(TilemapTerrain, ExactlyTheNineteenReachableConfigurationsAreValid) {
	std::set<uint8_t> reached;
	for (uint32_t key = 0; key < 4 * 4 * 4 * 4; ++key) {
		const CornerSet corners{static_cast<uint8_t>(key & 3U), static_cast<uint8_t>((key >> 2U) & 3U),
								static_cast<uint8_t>((key >> 4U) & 3U), static_cast<uint8_t>((key >> 6U) & 3U)};
		bool neighboursClose = true;
		for (size_t i = 0; i < 4; ++i)
			neighboursClose = neighboursClose && std::abs(int{corners[i]} - int{corners[(i + 1) % 4]}) <= 1;
		const auto slope = terrain::deriveSlope(corners);
		EXPECT_EQ(slope.has_value(), neighboursClose) << key;
		if (slope.has_value())
			reached.insert(*slope);
	}
	EXPECT_EQ(reached.size(), 19u);
}

TEST_F(TilemapTerrain, FlatMapHasNoHeights) {
	const auto map = makeMap(6, 4);
	EXPECT_FALSE(map.hasHeights());
	EXPECT_TRUE(map.getCornerHeights().empty());
	EXPECT_EQ(map.getCellSlope(2, 2), terrain::flatSlope);
	EXPECT_EQ(map.getCornerHeight(99, 99), 0);
	EXPECT_EQ(map.getMaxCornerHeight(), 0);
}

TEST_F(TilemapTerrain, CellSlopeFollowsTheCornerHeights) {
	for (uint8_t slope = 0; slope < terrain::slopeCount; ++slope) {
		const auto map = makeSlopeMap(slope, 2);
		EXPECT_EQ(map.getCellSlope(1, 1), slope) << terrain::getSlopeName(slope);
		EXPECT_EQ(map.getCellBaseHeight(1, 1), 2);
		EXPECT_TRUE(isValid(map));
	}
}

TEST_F(TilemapTerrain, RaisingACornerPullsItsNeighboursUp) {
	auto map = makeMap(6, 6);
	const auto changes = map.setCornerHeight(3, 3, 3);
	EXPECT_EQ(map.getCornerHeight(3, 3), 3);
	EXPECT_EQ(map.getCornerHeight(2, 3), 2);
	EXPECT_EQ(map.getCornerHeight(3, 4), 2);
	EXPECT_EQ(map.getCornerHeight(1, 3), 1);
	EXPECT_EQ(map.getCornerHeight(4, 4), 1);
	EXPECT_EQ(map.getCornerHeight(0, 3), 0);
	EXPECT_TRUE(isValid(map));
	EXPECT_EQ(map.getMaxCornerHeight(), 3);
	EXPECT_FALSE(changes.empty());
	map.applyCornerChanges(changes, true);
	EXPECT_FALSE(map.hasHeights());
	map.applyCornerChanges(changes, false);
	EXPECT_EQ(map.getCornerHeight(3, 3), 3);
}

TEST_F(TilemapTerrain, LoweringACornerPullsItsNeighboursDown) {
	auto map = makeMap(6, 6);
	std::ignore = map.levelArea({0, 0}, {6, 6}, 4);
	EXPECT_EQ(map.getCornerHeight(5, 5), 4);
	std::ignore = map.setCornerHeight(3, 3, 0);
	EXPECT_EQ(map.getCornerHeight(3, 3), 0);
	EXPECT_EQ(map.getCornerHeight(3, 2), 1);
	EXPECT_EQ(map.getCornerHeight(4, 4), 2);
	EXPECT_TRUE(isValid(map));
	EXPECT_EQ(map.getMaxCornerHeight(), 4);
}

TEST_F(TilemapTerrain, RaiseAndLowerAreOneStepOnTheCorner) {
	auto map = makeMap(4, 4);
	EXPECT_TRUE(map.lowerCorner(2, 2).empty()) << "nothing under zero";
	EXPECT_EQ(map.raiseCorner(2, 2).size(), 1u);
	EXPECT_EQ(map.getCornerHeight(2, 2), 1);
	EXPECT_EQ(map.getCellSlope(1, 1), 3) << "the front corner of the cell is up";
	EXPECT_EQ(map.lowerCorner(2, 2).size(), 1u);
	EXPECT_FALSE(map.hasHeights()) << "a flat map drops its corner grid";
	EXPECT_TRUE(map.raiseCorner(9, 9).empty());
}

TEST_F(TilemapTerrain, AreaOperationsKeepTheMapValid) {
	auto map = makeMap(10, 8);
	const auto raised = map.raiseArea({2, 2}, {5, 4});
	EXPECT_FALSE(raised.empty());
	for (uint32_t y = 2; y <= 4; ++y) {
		for (uint32_t x = 2; x <= 5; ++x) EXPECT_EQ(map.getCornerHeight(x, y), 1);
	}
	EXPECT_TRUE(isValid(map));
	std::ignore = map.raiseArea({2, 2}, {5, 4});
	EXPECT_EQ(map.getCornerHeight(3, 3), 2);
	EXPECT_TRUE(isValid(map));
	std::ignore = map.lowerArea({0, 0}, {10, 8});
	EXPECT_EQ(map.getCornerHeight(3, 3), 1);
	EXPECT_TRUE(isValid(map));
	std::ignore = map.levelArea({1, 1}, {6, 5}, 3);
	for (uint32_t y = 1; y <= 5; ++y) {
		for (uint32_t x = 1; x <= 6; ++x) EXPECT_EQ(map.getCornerHeight(x, y), 3);
	}
	EXPECT_TRUE(isValid(map));
	EXPECT_EQ(map.getMaxCornerHeight(), maxOf(map));
}

TEST_F(TilemapTerrain, RandomTerraformingKeepsTheInvariants) {
	auto map = makeMap(9, 7);
	uint32_t state = 12345U;
	const auto next = [&state](const uint32_t iModulo) -> uint32_t {
		state = state * 1664525U + 1013904223U;
		return (state >> 16U) % iModulo;
	};
	for (int step = 0; step < 400; ++step) {
		const auto before = snapshot(map);
		const uint32_t x = next(10U);
		const uint32_t y = next(8U);
		std::vector<CornerHeightChange> changes;
		switch (next(5U)) {
			case 0:
				changes = map.raiseCorner(x, y);
				break;
			case 1:
				changes = map.lowerCorner(x, y);
				break;
			case 2:
				changes = map.setCornerHeight(x, y, static_cast<uint8_t>(next(6U)));
				break;
			case 3:
				changes = map.raiseArea({x, y}, {x + next(3U), y + next(3U)});
				break;
			default:
				changes = map.lowerArea({x, y}, {x + next(3U), y + next(3U)});
				break;
		}
		ASSERT_TRUE(isValid(map)) << step;
		ASSERT_EQ(map.getMaxCornerHeight(), maxOf(map)) << step;
		ASSERT_EQ(map.hasHeights(), maxOf(map) > 0) << step;
		for (uint32_t cy = 0; cy < map.height; ++cy) {
			for (uint32_t cx = 0; cx < map.width; ++cx)
				ASSERT_TRUE(terrain::deriveSlope(map.getCellCorners(cx, cy)).has_value()) << step;
		}
		const auto after = snapshot(map);
		map.applyCornerChanges(changes, true);
		ASSERT_EQ(snapshot(map), before) << "undo " << step;
		map.applyCornerChanges(changes, false);
		ASSERT_EQ(snapshot(map), after) << "redo " << step;
	}
}

TEST_F(TilemapTerrain, MergedChangesKeepFirstBeforeAndLastAfter) {
	std::vector<CornerHeightChange> changes{{.index = 4, .before = 0, .after = 1},
											{.index = 2, .before = 3, .after = 4},
											{.index = 4, .before = 1, .after = 2},
											{.index = 7, .before = 1, .after = 2},
											{.index = 7, .before = 2, .after = 1}};
	terrain::mergeCornerChanges(changes);
	ASSERT_EQ(changes.size(), 2u);
	EXPECT_EQ(changes[0], (CornerHeightChange{.index = 2, .before = 3, .after = 4}));
	EXPECT_EQ(changes[1], (CornerHeightChange{.index = 4, .before = 0, .after = 2}));
}

TEST_F(TilemapTerrain, SetCornerHeightsRepairsAndRefusesBadSizes) {
	auto map = makeMap(2, 2);
	EXPECT_FALSE(map.setCornerHeights(std::vector<uint8_t>(5, 1)));
	EXPECT_FALSE(map.hasHeights());
	std::vector<uint8_t> heights(9, 0);
	heights[4] = 5;
	EXPECT_TRUE(map.setCornerHeights(heights));
	EXPECT_TRUE(isValid(map));
	EXPECT_EQ(map.getCornerHeight(1, 1), 5);
	EXPECT_EQ(map.getCornerHeight(0, 1), 4);
	EXPECT_EQ(map.getCornerHeight(0, 0), 3);
	EXPECT_TRUE(map.setCornerHeights({}));
	EXPECT_FALSE(map.hasHeights());
}

TEST_F(TilemapTerrain, ResizeKeepsAndExtendsTheHeights) {
	auto map = makeMap(4, 4);
	std::ignore = map.setCornerHeight(1, 1, 2);
	std::ignore = map.setCornerHeight(4, 4, 1);
	map.resize(6, 5);
	EXPECT_TRUE(isValid(map));
	EXPECT_EQ(map.getCornerHeight(1, 1), 2);
	EXPECT_EQ(map.getCornerHeight(4, 4), 1);
	EXPECT_EQ(map.getCornerHeight(6, 5), 1) << "new corners copy the border";
	map.resize(2, 2);
	EXPECT_TRUE(isValid(map));
	EXPECT_EQ(map.getCornerHeight(1, 1), 2);
	EXPECT_EQ(map.getCornerHeights().size(), 9u);
}

TEST_F(TilemapTerrain, HeightsAreSavedAndLoaded) {
	auto map = makeMap(70, 40);
	std::ignore = map.setCornerHeight(35, 20, 4);
	std::ignore = map.raiseArea({60, 30}, {64, 33});
	const auto text = map.serializeToString("hills");
	EXPECT_NE(text.find("FormatVersion: 3"), std::string::npos);
	TilemapAsset reloaded;
	ASSERT_TRUE(reloaded.deserializeFromString(text));
	EXPECT_TRUE(reloaded.hasSameContent(map));
	EXPECT_EQ(reloaded.getCornerHeight(35, 20), 4);
	EXPECT_EQ(reloaded.getMaxCornerHeight(), 4);
	EXPECT_EQ(reloaded.serializeToString("hills"), text);
	auto flat = map;
	EXPECT_TRUE(flat.setCornerHeights({}));
	EXPECT_FALSE(flat.hasSameContent(map)) << "the heights are part of the content";
}

TEST_F(TilemapTerrain, FormatTwoFilesLoadFlatAndAreWrittenAsFormatThree) {
	auto map = makeMap(5, 4);
	const auto v3 = map.serializeToString("legacy");
	const auto parts = codec::splitFile(v3);
	// A format 2 payload ends after its layers: drop the height flag before the checksum and seal again.
	std::string payload{parts.payload.substr(0, parts.payload.size() - 5)};
	const uint32_t sealed = codec::checksum(payload);
	for (size_t i = 0; i < 4; ++i) payload.push_back(static_cast<char>((sealed >> (8U * i)) & 0xFFU));
	std::string header{parts.header};
	header.replace(header.find("FormatVersion: 3"), 16, "FormatVersion: 2");
	header.replace(header.find("payloadBytes: "), header.size() - header.find("payloadBytes: "),
				   "payloadBytes: " + std::to_string(payload.size()) + "\n");
	TilemapAsset loaded;
	ASSERT_TRUE(loaded.deserializeFromString(header + "...\n" + payload));
	EXPECT_FALSE(loaded.hasHeights());
	EXPECT_EQ(loaded.getTile(0, 3, 2), 1);
	const auto rewritten = loaded.serializeToString("legacy");
	EXPECT_NE(rewritten.find("FormatVersion: 3"), std::string::npos);
	EXPECT_EQ(rewritten, v3);
}

TEST_F(TilemapTerrain, BadHeightSectionsAreRefused) {
	auto map = makeMap(3, 3);
	std::ignore = map.raiseCorner(1, 1);
	const auto text = map.serializeToString("bad");
	const auto parts = codec::splitFile(text);
	const auto resealed = [&](std::string iPayload) -> std::string {
		const auto body = iPayload.substr(0, iPayload.size() - 4);
		iPayload.resize(body.size());
		for (size_t i = 0; i < 4; ++i)
			iPayload.push_back(static_cast<char>((codec::checksum(body) >> (8U * i)) & 0xFFU));
		return std::string{parts.header} + "...\n" + iPayload;
	};
	// 3x3 map: 16 corner heights, a flag of 1 before them, the checksum after.
	const size_t flagOffset = parts.payload.size() - 4 - 16 - 1;
	std::string badFlag{parts.payload};
	badFlag[flagOffset] = '\x02';
	TilemapAsset target;
	EXPECT_FALSE(target.deserializeFromString(resealed(badFlag)));
	std::string shortHeights{parts.payload.substr(0, parts.payload.size() - 6)};
	shortHeights += "xxxx";
	EXPECT_FALSE(target.deserializeFromString(resealed(shortHeights)));
	EXPECT_FALSE(target.hasHeights());
	std::vector<uint8_t> heights;
	std::vector<component::TilemapLayer> layers(1);
	layers[0].resize({3, 3});
	const auto fiveHeights = std::vector<uint8_t>(5, 1);
	EXPECT_FALSE(codec::decodePayload(codec::encodePayload(layers, fiveHeights), layers, heights, 16, true));
}

TEST_F(TilemapTerrain, PickingFindsTheCellOnEverySlope) {
	for (uint8_t slope = 0; slope < terrain::slopeCount; ++slope) {
		const auto map = makeSlopeMap(slope, 1);
		const math::vec3 origin{-1.f, 2.f, 0.25f};
		for (const auto& [u, v]: {std::pair{0.2f, 0.3f}, std::pair{0.7f, 0.2f}, std::pair{0.4f, 0.8f},
								  std::pair{0.85f, 0.9f}, std::pair{0.5f, 0.5f}}) {
			const float z = surfaceHeight(map, 1, 1, u, v);
			const math::vec3 target{origin.x() + 1.f + u, origin.y() + 1.f + v, origin.z() + z};
			const auto straight =
					terrain::pickTerrain(map, origin, target + math::vec3{0.f, 0.f, 6.f}, math::vec3{0.f, 0.f, -1.f});
			ASSERT_TRUE(straight.has_value()) << terrain::getSlopeName(slope);
			EXPECT_EQ(straight->cell.x(), 1u);
			EXPECT_EQ(straight->cell.y(), 1u);
			EXPECT_NEAR(straight->point.z(), target.z(), 1e-3f) << terrain::getSlopeName(slope);
			EXPECT_EQ(straight->corner.x(), 1u + (u >= 0.5f ? 1u : 0u));
			EXPECT_EQ(straight->corner.y(), 1u + (v >= 0.5f ? 1u : 0u));
			const math::vec3 slanted{0.5f, 0.5f, -1.f};
			const auto oblique = terrain::pickTerrain(map, origin, target - slanted * 4.f, slanted);
			ASSERT_TRUE(oblique.has_value()) << terrain::getSlopeName(slope);
			EXPECT_EQ(oblique->cell.x(), 1u) << terrain::getSlopeName(slope);
			EXPECT_EQ(oblique->cell.y(), 1u) << terrain::getSlopeName(slope);
			EXPECT_NEAR(oblique->point.x(), target.x(), 1e-3f);
			EXPECT_NEAR(oblique->point.y(), target.y(), 1e-3f);
			EXPECT_NEAR(oblique->point.z(), target.z(), 1e-3f);
		}
	}
}

TEST_F(TilemapTerrain, PickingThroughTheCameraReturnsTheSurfacePoint) {
	renderer::IsometricCamera camera;
	camera.setViewportSize({800u, 600u});
	camera.setTarget({2.f, 2.f, 0.f});
	const math::vec3 origin{0.f, 0.f, 0.f};
	for (const uint8_t slope: {uint8_t{0}, uint8_t{3}, uint8_t{9}, uint8_t{13}, uint8_t{16}, uint8_t{18}}) {
		const auto map = makeSlopeMap(slope, 0);
		const float z = surfaceHeight(map, 1, 1, 0.5f, 0.5f);
		const math::vec3 surface{1.5f, 1.5f, z};
		const auto hit = terrain::pickTerrain(map, camera, camera.worldToViewport(surface), origin);
		ASSERT_TRUE(hit.has_value()) << terrain::getSlopeName(slope);
		EXPECT_EQ(hit->cell.x(), 1u);
		EXPECT_EQ(hit->cell.y(), 1u);
		EXPECT_NEAR(hit->point.z(), z, 1e-2f);
	}
}

TEST_F(TilemapTerrain, PickingMissesOutsideTheMapAndFindsFlatCells) {
	const auto map = makeMap(4, 4);
	EXPECT_FALSE(terrain::pickTerrain(map, {}, {9.f, 1.f, 5.f}, {0.f, 0.f, -1.f}).has_value());
	EXPECT_FALSE(terrain::pickTerrain(map, {}, {1.f, 1.f, 5.f}, {0.f, 0.f, 1.f}).has_value()) << "an upward ray";
	EXPECT_FALSE(terrain::pickTerrain(map, {}, {-1.f, 1.5f, 1.f}, {0.f, 0.f, -1.f}).has_value());
	const auto hit = terrain::pickTerrain(map, {}, {2.25f, 3.75f, 5.f}, {0.f, 0.f, -1.f});
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->cell.x(), 2u);
	EXPECT_EQ(hit->cell.y(), 3u);
	EXPECT_NEAR(hit->point.z(), 0.f, 1e-4f);
	EXPECT_EQ(hit->corner.x(), 2u);
	EXPECT_EQ(hit->corner.y(), 4u);
}

TEST_F(TilemapTerrain, PickingSeesAHillBehindAFlatCell) {
	auto map = makeMap(8, 8);
	std::ignore = map.levelArea({4, 4}, {5, 5}, 4);
	const auto hit = terrain::pickTerrain(map, {}, {4.5f, 4.5f, 10.f}, {0.f, 0.f, -1.f});
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->point.z(), 2.f, 1e-4f);
	const math::vec3 slanted{0.5f, 0.5f, -1.f};
	const auto low = terrain::pickTerrain(map, {}, math::vec3{1.5f, 1.5f, 0.5f} - slanted * 3.f, slanted);
	ASSERT_TRUE(low.has_value());
	EXPECT_EQ(low->cell.x(), 1u);
}

TEST_F(TilemapTerrain, TilesetResolvesTilesBySlope) {
	Tileset tileset;
	EXPECT_EQ(tileset.resolveTile(5, 3), 5) << "no table: the id is the tile";
	SlopeSet grass;
	grass.id = 0;
	grass.tiles[0] = 10;
	grass.tiles[3] = 13;
	grass.foundations[3] = 23;
	tileset.slopeSets.push_back(grass);
	EXPECT_EQ(tileset.resolveTile(0, 0), 10);
	EXPECT_EQ(tileset.resolveTile(0, 3), 13);
	EXPECT_EQ(tileset.resolveTile(0, 4), 10) << "an unset slope falls back on the flat tile";
	EXPECT_EQ(tileset.resolveTile(0, 3, true), 23);
	EXPECT_EQ(tileset.resolveTile(0, 4, true), 10);
	EXPECT_EQ(tileset.resolveTile(7, 3), 7);
	EXPECT_EQ(tileset.resolveTile(0, 200), 0);
	ASSERT_NE(tileset.findSlopeSet(0), nullptr);
	EXPECT_EQ(tileset.findSlopeSet(1), nullptr);
}

TEST_F(TilemapTerrain, TilesetSlopeTablesRoundTrip) {
	Tileset tileset;
	tileset.kind = Tileset::Kind::Isometric;
	tileset.columns = 4;
	tileset.rows = 3;
	tileset.resize(4, 3);
	SlopeSet road;
	road.id = 3;
	for (uint32_t i = 0; i < terrain::slopeCount; ++i) road.tiles[i] = static_cast<int32_t>(i % 12);
	road.foundations[5] = 11;
	tileset.slopeSets.push_back(road);
	SlopeSet plain;
	plain.id = 1;
	plain.tiles[0] = 1;
	tileset.slopeSets.push_back(plain);
	const auto text = tileset.serializeToString("iso");
	EXPECT_NE(text.find("slopes:"), std::string::npos);
	Tileset loaded;
	ASSERT_TRUE(loaded.deserializeFromString(text));
	EXPECT_EQ(loaded.slopeSets, tileset.slopeSets);
	const Tileset without;
	EXPECT_EQ(without.serializeToString("iso").find("slopes:"), std::string::npos);
}
