/**
 * @file ProceduralSky_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/ProceduralSky.h>

#include <cmath>
#include <cstddef>
#include <numbers>

using namespace owl;

namespace {

auto luminance(const math::vec3& iColor) -> float {
	return 0.2126f * iColor.x() + 0.7152f * iColor.y() + 0.0722f * iColor.z();
}

auto lookAt(const renderer::FloatImage& iImage, const math::vec3& iDirection) -> math::vec3 {
	return renderer::Equirectangular::sample(iImage, iDirection.normalized());
}

}// namespace

TEST(ProceduralSky, TheImageHasTheAskedSizeAndOnlyFiniteValues) {
	const auto image = renderer::renderProceduralSky({}, {128, 64});
	ASSERT_EQ(image.size, (math::vec2ui{128, 64}));
	ASSERT_EQ(image.pixels.size(), 128u * 64u);
	for (const auto& pixel: image.pixels) {
		ASSERT_TRUE(std::isfinite(pixel.x()) && std::isfinite(pixel.y()) && std::isfinite(pixel.z()));
		ASSERT_GE(pixel.x(), 0.f);
	}
}

TEST(ProceduralSky, AHighSunGivesABlueZenithNearOne) {
	const renderer::SkyParameters noon{.sunDirection = math::vec3{0.f, 0.866f, 0.5f}, .discSize = 0.f};
	const auto image = renderer::renderProceduralSky(noon, {256, 128});
	const auto zenith = lookAt(image, {0.f, 1.f, 0.001f});
	EXPECT_NEAR(luminance(zenith), 1.f, 0.35f);
	EXPECT_GT(zenith.z(), zenith.x());
	EXPECT_GT(luminance(lookAt(image, {0.f, 0.05f, 1.f})), luminance(lookAt(image, {0.f, -0.5f, 1.f})));
}

TEST(ProceduralSky, TheNightIsFarDarkerThanTheDayAndTheDuskGlowsTowardsTheSun) {
	const auto day = renderer::renderProceduralSky({.sunDirection = {0.f, 0.8f, 0.6f}, .stars = 0.f}, {128, 64});
	const auto night = renderer::renderProceduralSky({.sunDirection = {0.f, -0.8f, 0.6f}, .stars = 0.f}, {128, 64});
	const math::vec3 up{0.f, 1.f, 0.001f};
	EXPECT_LT(luminance(lookAt(night, up)), luminance(lookAt(day, up)) * 0.05f);
	EXPECT_GT(luminance(lookAt(night, up)), 0.f);
	const auto dusk = renderer::renderProceduralSky({.sunDirection = {1.f, 0.01f, 0.f}, .discSize = 0.f}, {256, 128});
	const auto towards = lookAt(dusk, {1.f, 0.05f, 0.f});
	const auto away = lookAt(dusk, {-1.f, 0.05f, 0.f});
	EXPECT_GT(towards.x(), away.x());
	EXPECT_GT(towards.x(), towards.z());
}

TEST(ProceduralSky, TheImageHoldsNeitherDiscNorStars) {
	const math::vec3 sun = math::vec3{0.3f, 0.6f, -0.7f}.normalized();
	const auto withDisc =
			renderer::renderProceduralSky({.sunDirection = sun, .stars = 1.f, .discSize = 5.f}, {256, 128});
	const auto without =
			renderer::renderProceduralSky({.sunDirection = sun, .stars = 0.f, .discSize = 0.f}, {256, 128});
	ASSERT_EQ(withDisc.pixels.size(), without.pixels.size());
	for (size_t index = 0; index < withDisc.pixels.size(); ++index)
		ASSERT_NEAR(luminance(withDisc.pixels[index]), luminance(without.pixels[index]), 1e-6f);
}

TEST(ProceduralSky, TheBodiesFollowTheSunAndTheNight) {
	const auto noon = renderer::skyBodiesOf({.sunDirection = {0.f, 2.f, 0.f}, .stars = 1.f, .discSize = 2.f});
	EXPECT_NEAR(noon.sunDirection.y(), 1.f, 1e-6f);
	EXPECT_NEAR(noon.discRadius, std::numbers::pi_v<float> / 180.f, 1e-6f);
	EXPECT_GT(noon.sunRadiance, 1.f);
	EXPECT_NEAR(noon.moonRadiance, 0.f, 1e-6f);
	EXPECT_NEAR(noon.starRadiance, 0.f, 1e-6f);
	const auto night = renderer::skyBodiesOf({.sunDirection = {0.f, -1.f, 0.f}, .stars = 1.f});
	EXPECT_NEAR(night.sunRadiance, 0.f, 1e-6f);
	EXPECT_GT(night.moonRadiance, 0.f);
	EXPECT_GT(night.starRadiance, 0.f);
	EXPECT_NEAR(renderer::skyBodiesOf({.sunDirection = {0.f, -1.f, 0.f}, .stars = 0.f}).starRadiance, 0.f, 1e-6f);
	EXPECT_NEAR(renderer::skyBodiesOf({.discSize = 0.f}).discRadius, 0.f, 1e-6f);
}

TEST(ProceduralSky, ADegenerateSunPointsUp) {
	const auto image = renderer::renderProceduralSky({.sunDirection = {0.f, 0.f, 0.f}}, {64, 32});
	for (const auto& pixel: image.pixels) ASSERT_TRUE(std::isfinite(pixel.y()));
}
