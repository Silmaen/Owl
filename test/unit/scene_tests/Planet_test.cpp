/**
 * @file Planet_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>
#include <scene/pcg/PcgGraph.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

constexpr auto g_heightGraph = R"(FormatVersion: 1
PcgGraph:
  nodes:
    - id: 1
      type: source.gradient
      position: [0, 0]
      values:
        direction: radial
    - id: 2
      type: field.invert
      position: [200, 0]
      values: {}
    - id: 3
      type: output.height
      position: [400, 0]
      values: {}
  links:
    - {from: 1, fromPin: Field, to: 2, toPin: Field}
    - {from: 2, fromPin: Field, to: 3, toPin: Height}
)";

constexpr auto g_tileGraph = R"(FormatVersion: 1
PcgGraph:
  nodes:
    - id: 1
      type: source.constant
      position: [0, 0]
      values:
        value: 1
    - id: 2
      type: output.tiles
      position: [200, 0]
      values:
        tile: 0
  links:
    - {from: 1, fromPin: Field, to: 2, toPin: Mask}
)";

class PlanetTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

}// namespace

TEST_F(PlanetTest, AGraphWritesTheHeightsOfThePlanet) {
	const auto map = component::evaluatePlanetHeights(g_heightGraph, "heights", 16, 5);
	ASSERT_EQ(map.size, (math::vec2ui{16, 16}));
	ASSERT_EQ(map.values.size(), 256u);
	EXPECT_TRUE(
			std::ranges::all_of(map.values, [](const float iValue) -> bool { return iValue >= 0.f && iValue <= 1.f; }));
	// An inverted radial gradient: high in the middle, low at the corners.
	EXPECT_GT(map.values[8 * 16 + 8], map.values[0]);
	EXPECT_TRUE(component::evaluatePlanetHeights(g_tileGraph, "tiles", 16, 5).values.empty());
	EXPECT_TRUE(component::evaluatePlanetHeights("not: [a graph", "broken", 16, 5).values.empty());
	const auto graph = pcg::deserialize(g_heightGraph, "heights");
	ASSERT_TRUE(graph);
	const auto result = pcg::evaluate(*graph, {.size = {8, 8}, .seed = 1});
	ASSERT_TRUE(result);
	EXPECT_EQ(result->height.size, (math::vec2ui{8, 8}));
	EXPECT_TRUE(result->warnings.empty());
}

TEST_F(PlanetTest, TheComponentRoundTripsThroughTheSceneFile) {
	const auto scene = mkShared<Scene>();
	auto entity = scene->createEntityWithUUID(11, "World");
	auto& planet = entity.addComponent<component::Planet>();
	planet.graph = "pcg/planet.owlpcg";
	planet.seed = 99;
	planet.heightResolution = 64;
	planet.settings.radius = 6.4e6;
	planet.settings.atmosphere = false;
	planet.settings.lowColor = {0.1f, 0.2f, 0.3f};
	planet.settings.scaledSpaceDistance = 5e7;
	const auto text = SceneSerializer(scene).serializeToString();
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>{text.begin(), text.end()}));
	const auto copy = loaded->findEntityByUUID(11);
	ASSERT_TRUE(copy.hasComponent<component::Planet>());
	const auto& read = copy.getComponent<component::Planet>();
	EXPECT_EQ(read.graph, "pcg/planet.owlpcg");
	EXPECT_EQ(read.seed, 99u);
	EXPECT_EQ(read.heightResolution, 64u);
	EXPECT_EQ(read.settings, planet.settings.clamped());
}

TEST_F(PlanetTest, TheSurfaceIsMadeAgainOnlyWhenItsSourceChanges) {
	const auto dir = std::filesystem::temp_directory_path() / "planetTest";
	std::filesystem::create_directories(dir);
	std::ofstream{dir / "heights.owlpcg"} << g_heightGraph;
	component::Planet planet;
	planet.graph = (dir / "heights.owlpcg").string();
	planet.heightResolution = 16;
	const auto first = planet.getSurface();
	ASSERT_TRUE(first);
	EXPECT_EQ(first->getHeights().size, (math::vec2ui{16, 16}));
	EXPECT_EQ(planet.getSurface(), first);
	planet.settings.heightScale = 10.f;
	const auto second = planet.getSurface();
	EXPECT_NE(second, first);
	EXPECT_EQ(second->getHeights(), first->getHeights());
	planet.seed = 3;
	EXPECT_NE(planet.getSurface(), second);
	auto& terrain = planet.getTerrain();
	EXPECT_EQ(terrain.getSurface(), planet.getSurface());
	// A copy builds its own terrain.
	const component::Planet copy = planet;
	EXPECT_FALSE(copy.m_runtime.terrain);
	EXPECT_FALSE(copy.m_runtime.surface);
	planet.graph = (dir / "missing.owlpcg").string();
	EXPECT_TRUE(planet.getSurface()->getHeights().values.empty());
	std::filesystem::remove_all(dir);
}

TEST_F(PlanetTest, ThePlanetCentreIsItsWorldPosition) {
	auto scene = mkShared<Scene>();
	scene->shiftWorldOrigin({1e9, 0.0, 0.0});
	auto planet = scene->createEntity("Planet");
	planet.addComponent<component::Planet>();
	planet.getComponent<component::Transform>().transform.translation() = {10.f, 0.f, 0.f};
	EXPECT_DOUBLE_EQ(scene->getEntityWorldPosition(planet).x(), 1e9 + 10.0);
	planet.addComponent<component::WorldPosition>().position = {2e9, 5.0, 0.0};
	EXPECT_DOUBLE_EQ(scene->getEntityWorldPosition(planet).x(), 2e9);
	EXPECT_DOUBLE_EQ(scene->getEntityWorldPosition(planet).y(), 5.0);
}
