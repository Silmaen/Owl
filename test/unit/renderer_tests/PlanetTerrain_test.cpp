/**
 * @file PlanetTerrain_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/task/Scheduler.h>
#include <renderer/PlanetTerrain.h>
#include <renderer/RendererPlanet.h>
#include <renderer/RendererSet.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::renderer;

namespace {

auto toDouble(const math::vec3& iVector) -> math::vec3d {
	return {static_cast<double>(iVector.x()), static_cast<double>(iVector.y()), static_cast<double>(iVector.z())};
}

auto flatSettings() -> PlanetSettings {
	PlanetSettings settings;
	settings.radius = 1000.0;
	settings.detailAmplitude = 0.f;
	settings.maxLevel = 6;
	settings.chunkResolution = 8;
	return settings;
}

auto isAncestor(const PlanetChunkKey& iAncestor, PlanetChunkKey iNode) -> bool {
	while (iNode.level > iAncestor.level) iNode = iNode.getParent();
	return iNode == iAncestor;
}

// Area of the leaves in face units: the leaves cover the sphere once when the sum is the number of faces.
auto coveredFaces(const std::vector<PlanetChunkKey>& iLeaves) -> double {
	double sum = 0.0;
	for (const auto& leaf: iLeaves) sum += std::ldexp(1.0, -2 * static_cast<int>(leaf.level));
	return sum;
}

}// namespace

TEST(PlanetTerrain, CubeFacesMapOntoTheUnitSphere) {
	for (uint8_t face = 0; face < g_planetFaces; ++face) {
		for (const double u: {-1.0, -0.3, 0.0, 0.7, 1.0}) {
			for (const double v: {-1.0, 0.5, 1.0}) EXPECT_NEAR(cubeToSphere(face, u, v).norm(), 1.0, 1e-12);
		}
	}
	EXPECT_NEAR(cubeToSphere(0, 0.0, 0.0).x(), 1.0, 1e-12);
	EXPECT_NEAR(cubeToSphere(5, 0.0, 0.0).z(), -1.0, 1e-12);
	// The +X face at its left edge and the +Z face at its right edge share their points.
	for (const double v: {-1.0, -0.2, 0.6}) {
		const auto a = cubeToSphere(0, -1.0, v);
		const auto b = cubeToSphere(4, 1.0, v);
		EXPECT_NEAR((a - b).norm(), 0.0, 1e-12);
	}
}

TEST(PlanetTerrain, KeysPackAndClimbToTheirParent) {
	const PlanetChunkKey key{.face = 3, .level = 4, .x = 9, .y = 13};
	EXPECT_EQ(key.getParent(), (PlanetChunkKey{.face = 3, .level = 3, .x = 4, .y = 6}));
	EXPECT_NE(key.packed(), key.getParent().packed());
	EXPECT_NE(key.packed(), (PlanetChunkKey{.face = 2, .level = 4, .x = 9, .y = 13}).packed());
	const PlanetChunkKey root{.face = 1, .level = 0, .x = 0, .y = 0};
	EXPECT_EQ(root.getParent(), root);
	EXPECT_NEAR(getChunkSize(root, 1000.0), 1000.0 * std::numbers::pi / 2.0, 1e-9);
}

TEST(PlanetTerrain, AFarEyeSelectsTheSixRoots) {
	std::vector<PlanetChunkKey> leaves;
	selectPlanetChunks(flatSettings(), {1e7, 0.0, 0.0}, leaves);
	ASSERT_EQ(leaves.size(), 6u);
	for (const auto& leaf: leaves) EXPECT_EQ(leaf.level, 0);
}

TEST(PlanetTerrain, ANearEyeRefinesUnderItAndCoversTheSphereOnce) {
	auto settings = flatSettings();
	std::vector<PlanetChunkKey> leaves;
	selectPlanetChunks(settings, {0.0, 1002.0, 0.0}, leaves);
	EXPECT_GT(leaves.size(), 6u);
	EXPECT_NEAR(coveredFaces(leaves), 6.0, 1e-9);
	uint8_t deepest = 0;
	PlanetChunkKey deepestKey;
	for (const auto& leaf: leaves) {
		EXPECT_LE(leaf.level, settings.maxLevel);
		if (leaf.level > deepest) {
			deepest = leaf.level;
			deepestKey = leaf;
		}
	}
	EXPECT_EQ(deepest, settings.maxLevel);
	EXPECT_EQ(deepestKey.face, 2);// +Y, under the eye.
	for (size_t i = 0; i < leaves.size(); ++i)
		for (size_t j = 0; j < leaves.size(); ++j) {
			if (i != j) {
				EXPECT_FALSE(isAncestor(leaves[i], leaves[j]));
			}
		}
	settings.maxLevel = 2;
	selectPlanetChunks(settings, {0.0, 1002.0, 0.0}, leaves);
	EXPECT_TRUE(std::ranges::all_of(leaves, [](const PlanetChunkKey& iLeaf) -> bool { return iLeaf.level <= 2; }));
}

TEST(PlanetTerrain, AChunkMeshesItsGridAndSkirtsOutward) {
	const PlanetSurface surface{flatSettings(), {}, 1};
	const PlanetChunkKey key{.face = 4, .level = 2, .x = 1, .y = 2};
	const auto mesh = buildPlanetChunk(surface, key);
	const uint32_t res = surface.getSettings().chunkResolution;
	EXPECT_EQ(mesh.vertices.size(), size_t{res + 1} * (res + 1) + size_t{4} * (res + 1));
	EXPECT_EQ(mesh.indices.size(), size_t{6} * res * res + size_t{24} * res);
	EXPECT_TRUE(std::ranges::all_of(mesh.indices,
									[&](const uint32_t iIndex) -> bool { return iIndex < mesh.vertices.size(); }));
	// A flat planet: the ground at the radius (the height map of a missing graph is 0.5, above the sea).
	const double expected = 1000.0 + surface.getHeightOf(0.5f);
	for (size_t index = 0; index < size_t{res + 1} * (res + 1); ++index) {
		const auto& p = mesh.vertices[index].position;
		const math::vec3d world = mesh.center + toDouble(p);
		EXPECT_NEAR(world.norm(), expected, 1e-2);
		EXPECT_EQ(mesh.vertices[index].textureIndex, 1u);
	}
	// The first triangle faces away from the centre.
	const auto& a = mesh.vertices[mesh.indices[0]].position;
	const auto& b = mesh.vertices[mesh.indices[1]].position;
	const auto& c = mesh.vertices[mesh.indices[2]].position;
	const math::vec3 normal = (b - a) ^ (c - a);
	const math::vec3d outward = mesh.center + toDouble(a);
	EXPECT_GT(static_cast<double>(normal.x()) * outward.x() + static_cast<double>(normal.y()) * outward.y() +
					  static_cast<double>(normal.z()) * outward.z(),
			  0.0);
	EXPECT_NEAR(mesh.radiusRange.x(), expected, 1e-2);
	EXPECT_NEAR(mesh.radiusRange.y(), expected, 1e-2);
}

TEST(PlanetTerrain, HeightsComeFromTheMapAndTheSeaFlattensTheLowGround) {
	PlanetHeightMap map{.size = {4, 4}, .values = std::vector<float>(16, 0.8f)};
	EXPECT_FLOAT_EQ(map.sample({0.0, 1.0, 0.0}), 0.8f);
	EXPECT_FLOAT_EQ(map.sample(math::vec3d{1.0, 1.0, 1.0}.normalized()), 0.8f);
	EXPECT_FLOAT_EQ(PlanetHeightMap{}.sample({1.0, 0.0, 0.0}), 0.5f);
	auto settings = flatSettings();
	settings.heightScale = 100.f;
	settings.seaLevel = 0.3f;
	const PlanetSurface high{settings, map, 7};
	EXPECT_NEAR(high.getSurfaceRadius({0.0, 0.0, 1.0}), 1000.0 + 50.0, 1e-3);
	map.values.assign(16, 0.1f);
	const PlanetSurface low{settings, map, 7};
	EXPECT_NEAR(low.getSurfaceRadius({0.0, 0.0, 1.0}), 1000.0, 1e-9);
	EXPECT_DOUBLE_EQ(low.getHeightOf(0.1f), 0.0);
}

TEST(PlanetTerrain, TheMapIsSampledWithoutSeams) {
	PlanetHeightMap map{.size = {32, 32}, .values = {}};
	for (uint32_t y = 0; y < 32; ++y)
		for (uint32_t x = 0; x < 32; ++x) map.values.push_back(static_cast<float>(x + y) / 62.f);
	// Across every plane where a projection changes side, neighbouring directions give neighbouring values.
	for (const auto& [a, b]: {std::pair{math::vec3d{1e-4, 0.6, 0.8}, math::vec3d{-1e-4, 0.6, 0.8}},
							  std::pair{math::vec3d{0.6, 1e-4, 0.8}, math::vec3d{0.6, -1e-4, 0.8}},
							  std::pair{math::vec3d{0.6, 0.8, 1e-4}, math::vec3d{0.6, 0.8, -1e-4}}}) {
		EXPECT_NEAR(map.sample(a.normalized()), map.sample(b.normalized()), 1e-3);
	}
}

TEST(PlanetTerrain, SettingsAreClamped) {
	PlanetSettings settings;
	settings.radius = -5.0;
	settings.maxLevel = 99;
	settings.chunkResolution = 1000;
	settings.seaLevel = 4.f;
	settings.lodFactor = std::numeric_limits<float>::quiet_NaN();
	settings.scaledSpaceDistance = 10.0;
	const auto clamped = settings.clamped();
	EXPECT_DOUBLE_EQ(clamped.radius, 1.0);
	EXPECT_EQ(clamped.maxLevel, g_planetMaxLevel);
	EXPECT_EQ(clamped.chunkResolution, g_planetMaxChunkResolution);
	EXPECT_FLOAT_EQ(clamped.seaLevel, 1.f);
	EXPECT_FLOAT_EQ(clamped.lodFactor, PlanetSettings{}.lodFactor);
	EXPECT_DOUBLE_EQ(clamped.scaledSpaceDistance, 10.0);
	EXPECT_EQ(clamped, clamped.clamped());
	EXPECT_NE(clamped, settings);
}

TEST(PlanetTerrain, AFarPlanetGoesToScaledSpaceAndKeepsItsAngularSize) {
	auto settings = flatSettings();
	settings.scaledSpaceDistance = 20000.0;
	const auto near = placePlanet(settings, {0.0, 0.0, -5000.0});
	EXPECT_FALSE(near.scaledSpace);
	EXPECT_DOUBLE_EQ(near.scale, 1.0);
	const math::vec3d far{3e6, 0.0, -4e6};
	const auto placed = placePlanet(settings, far);
	ASSERT_TRUE(placed.scaledSpace);
	EXPECT_NEAR(placed.center.norm(), 20000.0, 1e-6);
	EXPECT_NEAR(settings.radius * placed.scale / placed.center.norm(), settings.radius / far.norm(), 1e-12);
	settings.scaledSpaceDistance = 0.0;
	EXPECT_FALSE(placePlanet(settings, far).scaledSpace);
}

TEST(PlanetTerrain, TheColorRampGoesFromTheSeaToThePeaks) {
	auto settings = flatSettings();
	settings.seaColor = {0.f, 0.f, 1.f};
	settings.peakColor = {1.f, 1.f, 1.f};
	const auto ramp = buildPlanetColorRamp(settings, 64);
	ASSERT_EQ(ramp.size(), 64u);
	EXPECT_EQ(ramp.back(), 0xffffffffu);
	EXPECT_EQ(ramp.front() & 0xffffu, 0u);// no red nor green in the deep sea.
	EXPECT_EQ(ramp.front() >> 24u, 0xffu);
}

TEST(PlanetTerrain, TheTerrainBuildsWithoutSchedulerAndNeverOverlaps) {
	PlanetTerrain terrain;
	terrain.update({0.0, 2000.0, 0.0}, false, nullptr);
	EXPECT_TRUE(terrain.getDrawList().empty());
	terrain.setSurface(mkShared<const PlanetSurface>(flatSettings(), PlanetHeightMap{}, 3));
	for (int frame = 0; frame < 40; ++frame) terrain.update({0.0, 1003.0, 0.0}, false, nullptr);
	const auto& stats = terrain.getStats();
	EXPECT_GT(stats.selected, 6u);
	EXPECT_EQ(stats.drawn, stats.selected);
	EXPECT_EQ(stats.pending, 0u);
	const auto list = terrain.getDrawList();
	for (const auto& a: list)
		for (const auto& b: list) {
			if (!(a.key == b.key)) {
				EXPECT_FALSE(isAncestor(a.key, b.key));
			}
		}
	terrain.update({0.0, 1e7, 0.0}, true, nullptr);
	EXPECT_EQ(terrain.getStats().selected, 6u);
	EXPECT_EQ(terrain.getDrawList().size(), 6u);
}

TEST(PlanetTerrain, TheTerrainBuildsOnTheSchedulerWorkers) {
	core::task::Scheduler scheduler;
	PlanetTerrain terrain;
	terrain.setSurface(mkShared<const PlanetSurface>(flatSettings(), PlanetHeightMap{}, 3));
	terrain.update({0.0, 1003.0, 0.0}, false, &scheduler);
	EXPECT_GT(terrain.getStats().pending, 0u);
	for (int frame = 0; frame < 20 && terrain.getStats().drawn < terrain.getStats().selected; ++frame) {
		terrain.waitForBuilds(&scheduler);
		terrain.update({0.0, 1003.0, 0.0}, false, &scheduler);
	}
	EXPECT_EQ(terrain.getStats().drawn, terrain.getStats().selected);
	EXPECT_GT(terrain.getStats().built, 6u);
	// Another surface drops every chunk.
	auto settings = flatSettings();
	settings.radius = 500.0;
	terrain.setSurface(mkShared<const PlanetSurface>(settings, PlanetHeightMap{}, 3));
	terrain.update({0.0, 503.0, 0.0}, false, &scheduler);
	EXPECT_TRUE(terrain.getDrawList().empty());
	terrain.waitForBuilds(&scheduler);
}

TEST(PlanetTerrain, ThePlanetLayerBelongsToThe3DFamily) {
	EXPECT_TRUE(RendererSet::forLayerType("RendererPlanet").renderer3D);
	const auto folders = RendererSet{.raycast = false, .isometric = false, .renderer3D = true}.getShaderFolders();
	EXPECT_NE(std::ranges::find(folders, "planet"), folders.end());
	EXPECT_FALSE(RendererPlanet::isReady());
	RendererPlanet::resetStats();
	RendererPlanet::setSun({.toSun = {0.f, 0.f, 0.f}, .illuminance = {1.f, 1.f, 1.f}});
	EXPECT_FLOAT_EQ(RendererPlanet::getSun().toSun.y(), 1.f);
	RendererPlanet::endScene();
	EXPECT_EQ(RendererPlanet::getStats().atmospheres, 0u);
}
