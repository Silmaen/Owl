/**
 * @file PerlinNoise_test.cpp
 * @author Silmaen
 * @date 07/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/PerlinNoise.h>

using namespace owl;

TEST(PerlinNoise, DeterministicPerSeed) {
	const math::PerlinNoise a{42};
	const math::PerlinNoise b{42};
	for (float x = -3.f; x < 3.f; x += 0.37f) {
		EXPECT_FLOAT_EQ(a.noise(x, x * 0.5f), b.noise(x, x * 0.5f));
		EXPECT_FLOAT_EQ(a.noise(x, x * 0.5f, -x), b.noise(x, x * 0.5f, -x));
	}
}

TEST(PerlinNoise, DifferentSeedsDiffer) {
	const math::PerlinNoise a{1};
	const math::PerlinNoise b{2};
	float maxDiff = 0.f;
	for (float x = 0.f; x < 8.f; x += 0.5f)
		maxDiff = std::max(maxDiff, std::abs(a.noise(x + 0.3f, x * 0.7f) - b.noise(x + 0.3f, x * 0.7f)));
	EXPECT_GT(maxDiff, 0.1f);
}

TEST(PerlinNoise, ZeroAtIntegerLattice) {
	const math::PerlinNoise n{7};
	for (int32_t i = -4; i <= 4; ++i) {
		for (int32_t j = -4; j <= 4; ++j) {
			EXPECT_NEAR(n.noise(static_cast<float>(i), static_cast<float>(j)), 0.f, 1e-5f);
			EXPECT_NEAR(n.noise(static_cast<float>(i), static_cast<float>(j), static_cast<float>(i - j)), 0.f, 1e-5f);
		}
	}
}

TEST(PerlinNoise, BoundedAndVarying) {
	const math::PerlinNoise n{123};
	float lo = 1e9f;
	float hi = -1e9f;
	for (float x = 0.f; x < 32.f; x += 0.25f) {
		for (float y = 0.f; y < 32.f; y += 0.25f) {
			const float v = n.noise(x * 0.13f, y * 0.13f);
			EXPECT_LE(std::abs(v), 1.05f);
			lo = std::min(lo, v);
			hi = std::max(hi, v);
		}
	}
	EXPECT_GT(hi - lo, 0.5f);// field actually varies
}

TEST(PerlinNoise, Continuity) {
	const math::PerlinNoise n{99};
	for (float x = 0.f; x < 5.f; x += 0.5f) {
		const float a = n.noise(x, 1.234f);
		const float b = n.noise(x + 0.01f, 1.234f);
		EXPECT_LE(std::abs(a - b), 0.1f);
	}
}

TEST(PerlinNoise, FbmSingleOctaveEqualsNoise) {
	const math::PerlinNoise n{5};
	for (float x = -2.f; x < 2.f; x += 0.5f) {
		EXPECT_FLOAT_EQ(n.fbm(x, 0.5f, 1, 2.f, 0.5f), n.noise(x, 0.5f));
		EXPECT_FLOAT_EQ(n.fbm(x, 0.5f, -0.3f, 1, 2.f, 0.5f), n.noise(x, 0.5f, -0.3f));
	}
}

TEST(PerlinNoise, FbmBoundedAndDeterministic) {
	const math::PerlinNoise n{2024};
	for (float x = 0.f; x < 10.f; x += 0.5f) {
		const float v = n.fbm(x * 0.2f, x * 0.1f, 5, 2.f, 0.5f);
		EXPECT_LE(std::abs(v), 1.05f);
		EXPECT_FLOAT_EQ(v, n.fbm(x * 0.2f, x * 0.1f, 5, 2.f, 0.5f));
	}
}

TEST(PerlinNoise, ReseedChangesField) {
	math::PerlinNoise n{10};
	const float before = n.noise(1.5f, 2.5f);
	n.reseed(20);
	const float after = n.noise(1.5f, 2.5f);
	EXPECT_GT(std::abs(before - after), 1e-4f);
}
