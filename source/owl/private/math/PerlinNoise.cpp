/**
 * @file PerlinNoise.cpp
 * @author Silmaen
 * @date 07/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "math/PerlinNoise.h"

#include <cmath>
#include <numeric>
#include <random>

namespace owl::math {

namespace {
constexpr uint32_t g_DefaultSeed = 1337U;

auto fade(const float iT) -> float { return iT * iT * iT * (iT * (iT * 6.f - 15.f) + 10.f); }

auto lerp(const float iT, const float iA, const float iB) -> float { return iA + iT * (iB - iA); }

auto grad2(const uint8_t iHash, const float iX, const float iY) -> float {
	// Eight gradient directions (axis-aligned + normalized diagonals) keep the output near [-1, 1].
	constexpr float d = 0.70710678f;
	constexpr std::array<std::array<float, 2>, 8> grads{
			{{1.f, 0.f}, {-1.f, 0.f}, {0.f, 1.f}, {0.f, -1.f}, {d, d}, {-d, d}, {d, -d}, {-d, -d}}};
	const auto& g = grads[iHash & 7U];
	return g[0] * iX + g[1] * iY;
}

auto grad3(const uint8_t iHash, const float iX, const float iY, const float iZ) -> float {
	const uint8_t h = iHash & 15U;
	const float u = h < 8U ? iX : iY;
	float v = iZ;
	if (h < 4U)
		v = iY;
	else if (h == 12U || h == 14U)
		v = iX;
	return ((h & 1U) == 0U ? u : -u) + ((h & 2U) == 0U ? v : -v);
}
}// namespace

PerlinNoise::PerlinNoise() { reseed(g_DefaultSeed); }

PerlinNoise::PerlinNoise(const uint32_t iSeed) { reseed(iSeed); }

void PerlinNoise::reseed(const uint32_t iSeed) {
	std::array<uint8_t, 256> base{};
	std::iota(base.begin(), base.end(), static_cast<uint8_t>(0));
	std::shuffle(base.begin(), base.end(), std::mt19937{iSeed});
	for (size_t i = 0; i < 256; ++i) {
		m_perm[i] = base[i];
		m_perm[i + 256] = base[i];
	}
}

auto PerlinNoise::noise(const float iX, const float iY) const -> float {
	const auto xi = static_cast<int32_t>(std::floor(iX));
	const auto yi = static_cast<int32_t>(std::floor(iY));
	const float xf = iX - static_cast<float>(xi);
	const float yf = iY - static_cast<float>(yi);
	const auto gx = static_cast<size_t>(xi & 255);
	const auto gy = static_cast<size_t>(yi & 255);
	const float u = fade(xf);
	const float v = fade(yf);
	const uint8_t aa = m_perm[m_perm[gx] + gy];
	const uint8_t ba = m_perm[m_perm[gx + 1] + gy];
	const uint8_t ab = m_perm[m_perm[gx] + gy + 1];
	const uint8_t bb = m_perm[m_perm[gx + 1] + gy + 1];
	const float x1 = lerp(u, grad2(aa, xf, yf), grad2(ba, xf - 1.f, yf));
	const float x2 = lerp(u, grad2(ab, xf, yf - 1.f), grad2(bb, xf - 1.f, yf - 1.f));
	return lerp(v, x1, x2);
}

auto PerlinNoise::noise(const float iX, const float iY, const float iZ) const -> float {
	const auto xi = static_cast<int32_t>(std::floor(iX));
	const auto yi = static_cast<int32_t>(std::floor(iY));
	const auto zi = static_cast<int32_t>(std::floor(iZ));
	const float xf = iX - static_cast<float>(xi);
	const float yf = iY - static_cast<float>(yi);
	const float zf = iZ - static_cast<float>(zi);
	const auto gx = static_cast<size_t>(xi & 255);
	const auto gy = static_cast<size_t>(yi & 255);
	const auto gz = static_cast<size_t>(zi & 255);
	const float u = fade(xf);
	const float v = fade(yf);
	const float w = fade(zf);
	// Full-width indices (not wrapped to 8 bits); the doubled 512-entry table absorbs the offsets (max index 511).
	const size_t a = static_cast<size_t>(m_perm[gx]) + gy;
	const size_t b = static_cast<size_t>(m_perm[gx + 1]) + gy;
	const size_t aa = static_cast<size_t>(m_perm[a]) + gz;
	const size_t ab = static_cast<size_t>(m_perm[a + 1]) + gz;
	const size_t ba = static_cast<size_t>(m_perm[b]) + gz;
	const size_t bb = static_cast<size_t>(m_perm[b + 1]) + gz;
	const float x1 = lerp(u, grad3(m_perm[aa], xf, yf, zf), grad3(m_perm[ba], xf - 1.f, yf, zf));
	const float x2 = lerp(u, grad3(m_perm[ab], xf, yf - 1.f, zf), grad3(m_perm[bb], xf - 1.f, yf - 1.f, zf));
	const float y1 = lerp(v, x1, x2);
	const float x3 = lerp(u, grad3(m_perm[aa + 1], xf, yf, zf - 1.f), grad3(m_perm[ba + 1], xf - 1.f, yf, zf - 1.f));
	const float x4 =
			lerp(u, grad3(m_perm[ab + 1], xf, yf - 1.f, zf - 1.f), grad3(m_perm[bb + 1], xf - 1.f, yf - 1.f, zf - 1.f));
	const float y2 = lerp(v, x3, x4);
	return lerp(w, y1, y2);
}

auto PerlinNoise::fbm(const float iX, const float iY, const uint32_t iOctaves, const float iLacunarity,
					  const float iPersistence) const -> float {
	float total = 0.f;
	float amplitude = 1.f;
	float frequency = 1.f;
	float norm = 0.f;
	for (uint32_t octave = 0; octave < std::max(1U, iOctaves); ++octave) {
		total += amplitude * noise(iX * frequency, iY * frequency);
		norm += amplitude;
		amplitude *= iPersistence;
		frequency *= iLacunarity;
	}
	return norm > 0.f ? total / norm : 0.f;
}

auto PerlinNoise::fbm(const float iX, const float iY, const float iZ, const uint32_t iOctaves, const float iLacunarity,
					  const float iPersistence) const -> float {
	float total = 0.f;
	float amplitude = 1.f;
	float frequency = 1.f;
	float norm = 0.f;
	for (uint32_t octave = 0; octave < std::max(1U, iOctaves); ++octave) {
		total += amplitude * noise(iX * frequency, iY * frequency, iZ * frequency);
		norm += amplitude;
		amplitude *= iPersistence;
		frequency *= iLacunarity;
	}
	return norm > 0.f ? total / norm : 0.f;
}

}// namespace owl::math
