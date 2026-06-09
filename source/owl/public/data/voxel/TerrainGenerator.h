/**
 * @file TerrainGenerator.h
 * @author Silmaen
 * @date 07/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Block.h"
#include "Chunk.h"
#include "math/PerlinNoise.h"
#include "math/vectors.h"

namespace owl::data::voxel {

/**
 * @brief
 *  Configuration for procedural terrain generation.
 *
 * All fields are seed-reproducible inputs; the block ids let the generator stay
 * decoupled from a specific `BlockRegistry` (the caller passes the ids matching
 * its palette). A `water` id of `g_AirBlock` disables sea filling.
 */
struct TerrainParams {
	/// Seed driving both the height field and the cave field.
	uint32_t seed = 1337U;
	/// Horizontal frequency (scale) of the height field.
	float frequency = 0.02f;
	/// Number of fBm octaves for the height field.
	uint32_t octaves = 4U;
	/// Frequency multiplier between height octaves.
	float lacunarity = 2.f;
	/// Amplitude multiplier between height octaves.
	float persistence = 0.5f;
	/// World Y the terrain oscillates around.
	int32_t baseHeight = 0;
	/// Maximum height deviation from `baseHeight`.
	int32_t amplitude = 24;
	/// Water fills empty columns up to this world Y (set `water` to enable).
	int32_t seaLevel = 4;
	/// Thickness of the dirt layer under the surface before stone.
	int32_t dirtDepth = 4;
	/// 3D frequency of the cave field.
	float caveFrequency = 0.07f;
	/// Cave field threshold: solid blocks with cave noise above this are carved to air (>= 1 disables caves).
	float caveThreshold = 0.6f;
	/// Block id used for deep solid rock.
	BlockId stone = 1U;
	/// Block id used for the surface above sea level.
	BlockId grass = 2U;
	/// Block id used for the sub-surface layer.
	BlockId dirt = 3U;
	/// Block id used for the surface at or below sea level (beaches / sea floor).
	BlockId sand = 4U;
	/// Block id used to fill water up to `seaLevel`; `g_AirBlock` disables it.
	BlockId water = g_AirBlock;
	/// When true, a low-frequency biome field varies the surface block (desert / plains / snow / mountain).
	bool biomes = false;
	/// Frequency of the biome field (low -> large biome regions).
	float biomeFrequency = 0.006f;
	/// Surface block for the snowy biome; `g_AirBlock` falls back to grass.
	BlockId snow = g_AirBlock;
};

/**
 * @brief
 *  Seed-reproducible procedural voxel terrain generator.
 *
 * Builds a height field from 2D fractal Perlin noise (grass / dirt / stone
 * layering, sand at the shoreline, optional water up to the sea level) and
 * carves caves from a 3D Perlin field. Generation is per-chunk so it can feed
 * chunk streaming around the camera.
 */
class OWL_API TerrainGenerator {
public:
	TerrainGenerator(const TerrainGenerator&) = default;

	TerrainGenerator(TerrainGenerator&&) = default;

	auto operator=(const TerrainGenerator&) -> TerrainGenerator& = default;

	auto operator=(TerrainGenerator&&) -> TerrainGenerator& = default;

	/**
	 * @brief
	 *  Construct with the given parameters.
	 * @param[in] iParams The terrain parameters.
	 */
	explicit TerrainGenerator(const TerrainParams& iParams);

	/**
	 * @brief
	 *  Destructor.
	 */
	~TerrainGenerator() = default;

	/**
	 * @brief
	 *  Get the surface world height at a world column.
	 * @param[in] iWorldX World X column.
	 * @param[in] iWorldZ World Z column.
	 * @return The world Y of the topmost solid block in that column.
	 */
	[[nodiscard]] auto heightAt(int32_t iWorldX, int32_t iWorldZ) const -> int32_t;

	/**
	 * @brief
	 *  Fill a chunk with generated terrain for its coordinate.
	 * @param[in,out] ioChunk The chunk to fill (its coordinate is read from it).
	 */
	void generateChunk(Chunk& ioChunk) const;

	/**
	 * @brief
	 *  Fill a chunk with generated terrain at an explicit chunk coordinate.
	 * @param[in,out] ioChunk The chunk to fill.
	 * @param[in] iChunkCoord The chunk coordinate (world = coord * g_ChunkSize + local).
	 */
	void generateChunk(Chunk& ioChunk, const math::vec3i& iChunkCoord) const;

	/**
	 * @brief
	 *  Access the parameters.
	 * @return The terrain parameters.
	 */
	[[nodiscard]] auto getParams() const -> const TerrainParams& { return m_params; }

private:
	/// Terrain parameters.
	TerrainParams m_params;
	/// Height field noise.
	math::PerlinNoise m_height;
	/// Cave field noise (seeded distinctly from the height field).
	math::PerlinNoise m_cave;
	/// Biome field noise (seeded distinctly again), selects the surface block.
	math::PerlinNoise m_biome;

	/**
	 * @brief
	 *  Pick the surface block for a column from the biome field.
	 * @param[in] iWorldX World X column.
	 * @param[in] iWorldZ World Z column.
	 * @return The surface block id for that column's biome.
	 */
	[[nodiscard]] auto surfaceBlock(int32_t iWorldX, int32_t iWorldZ) const -> BlockId;
};

}// namespace owl::data::voxel
