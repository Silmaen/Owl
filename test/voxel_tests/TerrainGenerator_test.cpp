/**
 * @file TerrainGenerator_test.cpp
 * @author Silmaen
 * @date 07/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/voxel/TerrainGenerator.h>

#include <set>

using namespace owl;
using namespace owl::data::voxel;

namespace {
auto countBlock(const Chunk& iChunk, const BlockId iId) -> int32_t {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	int32_t count = 0;
	for (int32_t y = 0; y < size; ++y)
		for (int32_t z = 0; z < size; ++z)
			for (int32_t x = 0; x < size; ++x)
				if (iChunk.getBlock(x, y, z) == iId)
					++count;
	return count;
}
}// namespace

TEST(TerrainGenerator, HeightWithinAmplitude) {
	TerrainParams p;
	p.baseHeight = 30;
	p.amplitude = 12;
	const TerrainGenerator gen{p};
	for (int32_t x = -40; x <= 40; x += 7) {
		for (int32_t z = -40; z <= 40; z += 7) {
			const int32_t h = gen.heightAt(x, z);
			EXPECT_GE(h, p.baseHeight - p.amplitude);
			EXPECT_LE(h, p.baseHeight + p.amplitude);
		}
	}
}

TEST(TerrainGenerator, Deterministic) {
	TerrainParams p;
	p.seed = 4321U;
	const TerrainGenerator a{p};
	const TerrainGenerator b{p};
	Chunk ca{math::vec3i{1, 0, -2}};
	Chunk cb{math::vec3i{1, 0, -2}};
	a.generateChunk(ca);
	b.generateChunk(cb);
	EXPECT_EQ(ca.blocks(), cb.blocks());
}

TEST(TerrainGenerator, FarAboveIsAir) {
	TerrainParams p;
	p.baseHeight = 0;
	p.amplitude = 16;
	p.water = g_AirBlock;
	const TerrainGenerator gen{p};
	Chunk chunk{math::vec3i{0, 64, 0}};// world Y ~1024, far above any surface
	gen.generateChunk(chunk);
	EXPECT_TRUE(chunk.isEmpty());
}

TEST(TerrainGenerator, FarBelowIsSolidStone) {
	TerrainParams p;
	p.baseHeight = 0;
	p.amplitude = 16;
	p.caveThreshold = 2.f;// disable caves
	const TerrainGenerator gen{p};
	Chunk chunk{math::vec3i{0, -16, 0}};// world Y ~ -256, deep underground
	gen.generateChunk(chunk);
	EXPECT_EQ(countBlock(chunk, p.stone), static_cast<int32_t>(g_ChunkVolume));
}

TEST(TerrainGenerator, FlatTerrainLayering) {
	TerrainParams p;
	p.baseHeight = 10;
	p.amplitude = 0;// flat surface at y = 10
	p.seaLevel = 4;
	p.dirtDepth = 4;
	p.caveThreshold = 2.f;
	p.water = g_AirBlock;
	const TerrainGenerator gen{p};
	Chunk chunk{math::vec3i{0, 0, 0}};
	gen.generateChunk(chunk);
	EXPECT_EQ(chunk.getBlock(3, 10, 3), p.grass);// surface above sea -> grass
	EXPECT_EQ(chunk.getBlock(3, 9, 3), p.dirt);// depth 1 -> dirt
	EXPECT_EQ(chunk.getBlock(3, 6, 3), p.dirt);// depth 4 -> dirt
	EXPECT_EQ(chunk.getBlock(3, 5, 3), p.stone);// depth 5 -> stone
	EXPECT_EQ(chunk.getBlock(3, 11, 3), g_AirBlock);
}

TEST(TerrainGenerator, BeachSandAndWaterFill) {
	TerrainParams p;
	p.baseHeight = 2;
	p.amplitude = 0;// flat surface at y = 2, below sea level
	p.seaLevel = 4;
	p.caveThreshold = 2.f;
	p.water = 7U;
	const TerrainGenerator gen{p};
	Chunk chunk{math::vec3i{0, 0, 0}};
	gen.generateChunk(chunk);
	EXPECT_EQ(chunk.getBlock(5, 2, 5), p.sand);// surface below sea -> sand
	EXPECT_EQ(chunk.getBlock(5, 3, 5), p.water);// above ground, <= sea -> water
	EXPECT_EQ(chunk.getBlock(5, 4, 5), p.water);
	EXPECT_EQ(chunk.getBlock(5, 5, 5), g_AirBlock);// above sea -> air
}

TEST(TerrainGenerator, CavesCarveSolid) {
	TerrainParams p;
	p.baseHeight = 0;
	p.amplitude = 0;
	p.caveFrequency = 0.1f;
	const TerrainGenerator solid{[&] {
		TerrainParams q = p;
		q.caveThreshold = 2.f;// no caves
		return q;
	}()};
	const TerrainGenerator carved{[&] {
		TerrainParams q = p;
		q.caveThreshold = 0.3f;// generous caves
		return q;
	}()};
	Chunk a{math::vec3i{0, -4, 0}};
	Chunk b{math::vec3i{0, -4, 0}};
	solid.generateChunk(a);
	carved.generateChunk(b);
	EXPECT_GT(countBlock(b, g_AirBlock), countBlock(a, g_AirBlock));
}

TEST(TerrainGenerator, BiomesVarySurface) {
	TerrainParams p;
	p.baseHeight = 20;
	p.amplitude = 0;// flat surface at y = 20 (above sea) so the surface block is purely biome-driven
	p.seaLevel = 0;
	p.caveThreshold = 2.f;
	p.biomes = true;
	p.snow = 6;
	const TerrainGenerator gen{p};
	std::set<BlockId> surfaces;
	// Surface sits at world y = 20 -> chunk coord y = 1, local y = 4. Scan a wide XZ swath of columns.
	for (int32_t cx = 0; cx < 40; ++cx) {
		Chunk chunk{math::vec3i{cx, 1, 0}};
		gen.generateChunk(chunk);
		for (int32_t lx = 0; lx < static_cast<int32_t>(g_ChunkSize); ++lx) surfaces.insert(chunk.getBlock(lx, 4, 0));
	}
	EXPECT_GE(surfaces.size(), 2u);// biomes produce more than one surface block
	EXPECT_FALSE(surfaces.contains(g_AirBlock));// the surface is solid everywhere

	TerrainParams flat = p;
	flat.biomes = false;
	const TerrainGenerator plain{flat};
	Chunk c{math::vec3i{0, 1, 0}};
	plain.generateChunk(c);
	EXPECT_EQ(c.getBlock(3, 4, 3), flat.grass);// biomes off -> grass everywhere
}
