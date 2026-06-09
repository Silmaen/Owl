/**
 * @file PerlinNoise.h
 * @author Silmaen
 * @date 07/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <array>
#include <cstdint>

namespace owl::math {
/**
 * @brief
 *  Seeded Perlin gradient noise (Ken Perlin's improved noise), with fractal
 *  Brownian motion helpers.
 *
 * Self-contained and dependency-free; lives in `owl::math` so the voxel terrain
 * generator (and any other procedural system) can use it without pulling in a
 * third-party noise library. A given seed always produces the same field, so
 * worlds are reproducible. The raw `noise` samplers return values in roughly
 * `[-1, 1]`; the `fbm` helpers normalise their octave sum back to that range.
 */
class OWL_API PerlinNoise {
public:
	PerlinNoise(const PerlinNoise&) = default;

	PerlinNoise(PerlinNoise&&) = default;

	auto operator=(const PerlinNoise&) -> PerlinNoise& = default;

	auto operator=(PerlinNoise&&) -> PerlinNoise& = default;

	/**
	 * @brief
	 *  Construct with a fixed default seed.
	 */
	PerlinNoise();

	/**
	 * @brief
	 *  Construct with an explicit seed.
	 * @param[in] iSeed Seed driving the permutation table.
	 */
	explicit PerlinNoise(uint32_t iSeed);

	/**
	 * @brief
	 *  Destructor.
	 */
	~PerlinNoise() = default;

	/**
	 * @brief
	 *  Rebuild the permutation table from a new seed.
	 * @param[in] iSeed The new seed.
	 */
	void reseed(uint32_t iSeed);

	/**
	 * @brief
	 *  Sample 2D gradient noise.
	 * @param[in] iX X coordinate.
	 * @param[in] iY Y coordinate.
	 * @return Noise value in roughly `[-1, 1]`.
	 */
	[[nodiscard]] auto noise(float iX, float iY) const -> float;

	/**
	 * @brief
	 *  Sample 3D gradient noise.
	 * @param[in] iX X coordinate.
	 * @param[in] iY Y coordinate.
	 * @param[in] iZ Z coordinate.
	 * @return Noise value in roughly `[-1, 1]`.
	 */
	[[nodiscard]] auto noise(float iX, float iY, float iZ) const -> float;

	/**
	 * @brief
	 *  Sample 2D fractal Brownian motion (summed octaves).
	 * @param[in] iX X coordinate.
	 * @param[in] iY Y coordinate.
	 * @param[in] iOctaves Number of noise layers summed (>= 1).
	 * @param[in] iLacunarity Frequency multiplier between octaves.
	 * @param[in] iPersistence Amplitude multiplier between octaves.
	 * @return Noise value normalised to roughly `[-1, 1]`.
	 */
	[[nodiscard]] auto fbm(float iX, float iY, uint32_t iOctaves, float iLacunarity, float iPersistence) const -> float;

	/**
	 * @brief
	 *  Sample 3D fractal Brownian motion (summed octaves).
	 * @param[in] iX X coordinate.
	 * @param[in] iY Y coordinate.
	 * @param[in] iZ Z coordinate.
	 * @param[in] iOctaves Number of noise layers summed (>= 1).
	 * @param[in] iLacunarity Frequency multiplier between octaves.
	 * @param[in] iPersistence Amplitude multiplier between octaves.
	 * @return Noise value normalised to roughly `[-1, 1]`.
	 */
	[[nodiscard]] auto fbm(float iX, float iY, float iZ, uint32_t iOctaves, float iLacunarity, float iPersistence) const
			-> float;

private:
	/// Permutation table, doubled to 512 to avoid index wrapping in the lookups.
	std::array<uint8_t, 512> m_perm{};
};
}// namespace owl::math
