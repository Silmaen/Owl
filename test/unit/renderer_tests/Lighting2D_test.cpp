/**
 * @file Lighting2D_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/Lighting2D.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

using namespace owl;
using namespace owl::renderer;

namespace {
// Bin of the shadow row holding a direction.
auto binOfAngle(const float iAngle) -> size_t {
	const float turn = (iAngle + std::numbers::pi_v<float>) / (2.f * std::numbers::pi_v<float>);
	return static_cast<size_t>(turn * static_cast<float>(g_shadow2DBins)) % g_shadow2DBins;
}
}// namespace

TEST(Lighting2D, aShadowRowWithoutOccluderReachesTheRadius) {
	std::vector<float> row(g_shadow2DBins, 0.f);
	buildShadowRow2D({0.f, 0.f}, 6.f, {}, row);
	for (const float distance: row) EXPECT_FLOAT_EQ(distance, 6.f);
}

TEST(Lighting2D, aWallStopsTheBinsItCoversOnly) {
	// A vertical wall at x = 2, from y = -1 to y = 1: about +-26.6 degrees seen from the light at the origin.
	const std::array<Occluder2D, 1> wall{Occluder2D{.start = {2.f, -1.f}, .end = {2.f, 1.f}}};
	std::vector<float> row(g_shadow2DBins, 0.f);
	buildShadowRow2D({0.f, 0.f}, 10.f, wall, row);
	EXPECT_NEAR(row[binOfAngle(0.f)], 2.f, 1e-3f);
	EXPECT_NEAR(row[binOfAngle(0.4f)], 2.f / std::cos(0.4f), 0.05f);
	EXPECT_FLOAT_EQ(row[binOfAngle(0.6f)], 10.f);
	EXPECT_FLOAT_EQ(row[binOfAngle(std::numbers::pi_v<float> * 0.5f)], 10.f);
	EXPECT_FLOAT_EQ(row[binOfAngle(-std::numbers::pi_v<float> * 0.9f)], 10.f);
}

TEST(Lighting2D, aWallBehindTheLightWrapsAroundTheRowEnds) {
	// A wall at x = -2 crosses the angle +-pi, where the row starts and ends.
	const std::array<Occluder2D, 1> wall{Occluder2D{.start = {-2.f, -1.f}, .end = {-2.f, 1.f}}};
	std::vector<float> row(g_shadow2DBins, 0.f);
	buildShadowRow2D({0.f, 0.f}, 10.f, wall, row);
	EXPECT_NEAR(row.front(), 2.f, 1e-2f);
	EXPECT_NEAR(row.back(), 2.f, 1e-2f);
	EXPECT_FLOAT_EQ(row[binOfAngle(0.f)], 10.f);
}

TEST(Lighting2D, anOccluderBeyondTheRadiusIsIgnoredAndTheNearestWins) {
	const std::array<Occluder2D, 3> walls{Occluder2D{.start = {20.f, -1.f}, .end = {20.f, 1.f}},
										  Occluder2D{.start = {4.f, -1.f}, .end = {4.f, 1.f}},
										  Occluder2D{.start = {3.f, -1.f}, .end = {3.f, 1.f}}};
	std::vector<float> row(g_shadow2DBins, 0.f);
	buildShadowRow2D({0.f, 0.f}, 10.f, walls, row);
	EXPECT_NEAR(row[binOfAngle(0.f)], 3.f, 1e-3f);
	std::vector<float> far(g_shadow2DBins, 0.f);
	buildShadowRow2D({0.f, 0.f}, 10.f, std::span{walls}.first(1), far);
	EXPECT_FLOAT_EQ(far[binOfAngle(0.f)], 10.f);
}

TEST(Lighting2D, lightFadesToZeroAtTheRadius) {
	const Light2DData light{.position = {1.f, 1.f}, .radius = 4.f, .color = {2.f, 1.f, 0.5f}, .falloff = 1.f};
	EXPECT_EQ(lightAt2D(light, {1.f, 1.f}), (math::vec3{2.f, 1.f, 0.5f}));
	const auto half = lightAt2D(light, {3.f, 1.f});
	EXPECT_NEAR(half.x(), 1.f, 1e-5f);
	EXPECT_NEAR(half.z(), 0.25f, 1e-5f);
	EXPECT_EQ(lightAt2D(light, {5.5f, 1.f}), (math::vec3{0.f, 0.f, 0.f}));
}

TEST(Lighting2D, aSpotLightsItsConeOnly) {
	const Light2DData spot{.position = {0.f, 0.f},
						   .radius = 10.f,
						   .color = {1.f, 1.f, 1.f},
						   .falloff = 1.f,
						   .direction = {1.f, 0.f},
						   .cosInner = std::cos(0.3f),
						   .cosOuter = std::cos(0.5f)};
	EXPECT_GT(lightAt2D(spot, {2.f, 0.f}).x(), 0.7f);
	EXPECT_FLOAT_EQ(lightAt2D(spot, {0.f, 2.f}).x(), 0.f);
	EXPECT_FLOAT_EQ(lightAt2D(spot, {-2.f, 0.f}).x(), 0.f);
	const float edge = lightAt2D(spot, {2.f * std::cos(0.4f), 2.f * std::sin(0.4f)}).x();
	EXPECT_GT(edge, 0.f);
	EXPECT_LT(edge, lightAt2D(spot, {2.f, 0.f}).x());
}
