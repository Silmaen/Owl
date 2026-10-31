/**
 * @file ColorVision_test.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/matrices.h>
#include <renderer/ColorVision.h>

#include <array>
#include <cstddef>

using namespace owl;
using renderer::ColorDeficiency;
using renderer::ColorVisionFilter;
using renderer::ColorVisionMode;

namespace {

constexpr std::array g_deficiencies{ColorDeficiency::Protanopia, ColorDeficiency::Deuteranopia,
									ColorDeficiency::Tritanopia};

auto apply(const math::mat3& iMatrix, const std::array<float, 3>& iColor) -> std::array<float, 3> {
	std::array<float, 3> result{};
	for (size_t row = 0; row < 3; ++row)
		for (size_t col = 0; col < 3; ++col) result[row] += iMatrix(row, col) * iColor[col];
	return result;
}

void expectIdentity(const math::mat3& iMatrix) {
	for (size_t row = 0; row < 3; ++row)
		for (size_t col = 0; col < 3; ++col) EXPECT_FLOAT_EQ(iMatrix(row, col), row == col ? 1.f : 0.f);
}

}// namespace

TEST(ColorVision, anInactiveFilterIsTheIdentity) {
	EXPECT_FALSE(ColorVisionFilter{}.isActive());
	expectIdentity(ColorVisionFilter{}.getMatrix());
	const ColorVisionFilter still{.deficiency = ColorDeficiency::Protanopia, .strength = 0.f};
	EXPECT_FALSE(still.isActive());
	expectIdentity(still.getMatrix());
}

TEST(ColorVision, greysStayGreyInEveryMode) {
	for (const auto deficiency: g_deficiencies) {
		for (const auto mode: {ColorVisionMode::Correct, ColorVisionMode::Simulate}) {
			const ColorVisionFilter filter{.deficiency = deficiency, .mode = mode, .strength = 1.f};
			ASSERT_TRUE(filter.isActive());
			const auto grey = apply(filter.getMatrix(), {0.5f, 0.5f, 0.5f});
			for (const float channel: grey) EXPECT_NEAR(channel, 0.5f, 1e-3f);
		}
	}
}

TEST(ColorVision, aProtanopeConfusesRedAndGreenTheCorrectionSeparatesThem) {
	const ColorVisionFilter simulate{.deficiency = ColorDeficiency::Protanopia, .mode = ColorVisionMode::Simulate};
	const ColorVisionFilter correct{.deficiency = ColorDeficiency::Protanopia, .mode = ColorVisionMode::Correct};
	const auto seenRed = apply(simulate.getMatrix(), {1.f, 0.f, 0.f});
	// A protanope sees almost no red in pure red.
	EXPECT_LT(seenRed[0], 0.2f);
	// The correction moves the lost red contrast into green and blue.
	const auto corrected = apply(correct.getMatrix(), {1.f, 0.f, 0.f});
	EXPECT_FLOAT_EQ(corrected[0], 1.f);
	EXPECT_GT(corrected[2], 0.5f);
}

TEST(ColorVision, theStrengthBlendsWithTheIdentity) {
	const ColorVisionFilter full{.deficiency = ColorDeficiency::Deuteranopia, .mode = ColorVisionMode::Simulate};
	const ColorVisionFilter half{.deficiency = ColorDeficiency::Deuteranopia,
								 .mode = ColorVisionMode::Simulate,
								 .strength = 0.5f};
	const auto fullMatrix = full.getMatrix();
	const auto halfMatrix = half.getMatrix();
	for (size_t row = 0; row < 3; ++row)
		for (size_t col = 0; col < 3; ++col)
			EXPECT_NEAR(halfMatrix(row, col), 0.5f * (fullMatrix(row, col) + (row == col ? 1.f : 0.f)), 1e-6f);
	const ColorVisionFilter beyond{.deficiency = ColorDeficiency::Deuteranopia,
								   .mode = ColorVisionMode::Simulate,
								   .strength = 4.f};
	EXPECT_EQ(beyond.getMatrix(), fullMatrix);
}

TEST(ColorVision, namesRoundTrip) {
	for (const auto deficiency: {ColorDeficiency::None, ColorDeficiency::Protanopia, ColorDeficiency::Deuteranopia,
								 ColorDeficiency::Tritanopia})
		EXPECT_EQ(renderer::colorDeficiencyFromName(renderer::toName(deficiency)), deficiency);
	EXPECT_FALSE(renderer::colorDeficiencyFromName("Protanopia").has_value());
	EXPECT_FALSE(renderer::colorDeficiencyFromName("").has_value());
}
