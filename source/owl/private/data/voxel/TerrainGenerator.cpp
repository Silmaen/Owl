/**
 * @file TerrainGenerator.cpp
 * @author Silmaen
 * @date 07/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/TerrainGenerator.h"

#include <cmath>

namespace owl::data::voxel {

TerrainGenerator::TerrainGenerator(const TerrainParams& iParams)
	: m_params{iParams}, m_height{iParams.seed}, m_cave{iParams.seed + 1U}, m_biome{iParams.seed + 2U} {}

auto TerrainGenerator::surfaceBlock(const int32_t iWorldX, const int32_t iWorldZ) const -> BlockId {
	if (!m_params.biomes)
		return m_params.grass;
	const float b = m_biome.fbm(static_cast<float>(iWorldX) * m_params.biomeFrequency,
								static_cast<float>(iWorldZ) * m_params.biomeFrequency, 3U, 2.f, 0.5f);
	if (b < -0.4f)
		return m_params.sand;// desert
	if (b < 0.2f)
		return m_params.grass;// plains
	if (b < 0.6f)
		return m_params.snow != g_AirBlock ? m_params.snow : m_params.grass;// snowy
	return m_params.stone;// rocky mountain top
}

auto TerrainGenerator::heightAt(const int32_t iWorldX, const int32_t iWorldZ) const -> int32_t {
	const float n = m_height.fbm(static_cast<float>(iWorldX) * m_params.frequency,
								 static_cast<float>(iWorldZ) * m_params.frequency, m_params.octaves,
								 m_params.lacunarity, m_params.persistence);
	return m_params.baseHeight + static_cast<int32_t>(std::lround(n * static_cast<float>(m_params.amplitude)));
}

void TerrainGenerator::generateChunk(Chunk& ioChunk) const { generateChunk(ioChunk, ioChunk.getCoord()); }

void TerrainGenerator::generateChunk(Chunk& ioChunk, const math::vec3i& iChunkCoord) const {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	const int32_t baseX = iChunkCoord.x() * size;
	const int32_t baseY = iChunkCoord.y() * size;
	const int32_t baseZ = iChunkCoord.z() * size;
	const bool caves = m_params.caveThreshold < 1.f;
	ioChunk.fill(g_AirBlock);
	for (int32_t lz = 0; lz < size; ++lz) {
		for (int32_t lx = 0; lx < size; ++lx) {
			const int32_t worldX = baseX + lx;
			const int32_t worldZ = baseZ + lz;
			const int32_t height = heightAt(worldX, worldZ);
			for (int32_t ly = 0; ly < size; ++ly) {
				const int32_t worldY = baseY + ly;
				if (worldY > height) {
					if (m_params.water != g_AirBlock && worldY <= m_params.seaLevel)
						ioChunk.setBlock(lx, ly, lz, m_params.water);
					continue;
				}
				const int32_t depth = height - worldY;
				BlockId block = m_params.stone;
				if (depth == 0)
					block = worldY < m_params.seaLevel ? m_params.sand : surfaceBlock(worldX, worldZ);
				else if (depth <= m_params.dirtDepth)
					block = m_params.dirt;
				// Carve caves below the immediate surface only, so the ground crust stays intact.
				if (caves && depth > 1) {
					const float c = m_cave.noise(static_cast<float>(worldX) * m_params.caveFrequency,
												 static_cast<float>(worldY) * m_params.caveFrequency,
												 static_cast<float>(worldZ) * m_params.caveFrequency);
					if (c > m_params.caveThreshold)
						block = g_AirBlock;
				}
				if (block != g_AirBlock)
					ioChunk.setBlock(lx, ly, lz, block);
			}
		}
	}
}

}// namespace owl::data::voxel
