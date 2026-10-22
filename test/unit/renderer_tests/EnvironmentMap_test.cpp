/**
 * @file EnvironmentMap_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/EnvironmentMap.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

using namespace owl;
using renderer::ColorLut;
using renderer::EnvironmentMap;
using renderer::Equirectangular;
using renderer::FloatImage;
using renderer::SphericalHarmonics;

namespace {

auto makeImage(const uint32_t iWidth, const uint32_t iHeight,
			   const std::function<math::vec3(uint32_t, uint32_t)>& iFill) -> FloatImage {
	FloatImage image;
	image.size = {iWidth, iHeight};
	image.pixels.reserve(static_cast<size_t>(iWidth) * iHeight);
	for (uint32_t y = 0; y < iHeight; ++y) {
		for (uint32_t x = 0; x < iWidth; ++x) { image.pixels.push_back(iFill(x, y)); }
	}
	return image;
}

auto makeConstantImage(const uint32_t iWidth, const uint32_t iHeight, const math::vec3& iValue) -> FloatImage {
	return makeImage(iWidth, iHeight, [&](uint32_t, uint32_t) -> math::vec3 { return iValue; });
}

void expectNear(const math::vec3& iActual, const math::vec3& iExpected, const float iTolerance) {
	EXPECT_NEAR(iActual.x(), iExpected.x(), iTolerance);
	EXPECT_NEAR(iActual.y(), iExpected.y(), iTolerance);
	EXPECT_NEAR(iActual.z(), iExpected.z(), iTolerance);
}

auto identityCube() -> std::string {
	std::string text = "# identity\nTITLE \"id\"\nLUT_3D_SIZE 2\n";
	for (int b = 0; b < 2; ++b) {
		for (int g = 0; g < 2; ++g) {
			for (int r = 0; r < 2; ++r) {
				text += std::to_string(r) + " " + std::to_string(g) + " " + std::to_string(b) + "\n";
			}
		}
	}
	return text;
}

}// namespace

TEST(EnvironmentMap, EquirectangularAxes) {
	const auto up = Equirectangular::toUv({0.f, 1.f, 0.f});
	EXPECT_NEAR(up.y(), 0.f, 1e-6f);
	const auto down = Equirectangular::toUv({0.f, -1.f, 0.f});
	EXPECT_NEAR(down.y(), 1.f, 1e-6f);
	const auto minusZ = Equirectangular::toUv({0.f, 0.f, -1.f});
	EXPECT_NEAR(minusZ.x(), 0.5f, 1e-6f);
	EXPECT_NEAR(minusZ.y(), 0.5f, 1e-6f);
	const auto plusX = Equirectangular::toUv({1.f, 0.f, 0.f});
	EXPECT_NEAR(plusX.x(), 0.75f, 1e-6f);
	const auto minusX = Equirectangular::toUv({-1.f, 0.f, 0.f});
	EXPECT_NEAR(minusX.x(), 0.25f, 1e-6f);
	// +Z sits on the seam of the image: u = 1 (or 0 on the other side).
	const auto plusZ = Equirectangular::toUv({0.f, 0.f, 1.f});
	EXPECT_NEAR(std::abs(plusZ.x() - 0.5f), 0.5f, 1e-6f);
}

TEST(EnvironmentMap, EquirectangularRoundTrip) {
	const std::vector<math::vec3> directions = {
			{0.f, 1.f, 0.f},
			{0.f, -1.f, 0.f},
			{1.f, 0.f, 0.f},
			{-1.f, 0.f, 0.f},
			{0.f, 0.f, 1.f},
			{0.f, 0.f, -1.f},
			math::vec3{1.f, 2.f, -3.f}.normalized(),
			math::vec3{-0.3f, -0.5f, 0.8f}.normalized(),
	};
	for (const auto& dir: directions) {
		expectNear(Equirectangular::toDirection(Equirectangular::toUv(dir)), dir, 1e-5f);
	}
}

TEST(EnvironmentMap, EquirectangularSampleAtPixelCentre) {
	const auto image = makeImage(4, 2, [](const uint32_t iX, const uint32_t iY) -> math::vec3 {
		return {static_cast<float>(iX), static_cast<float>(iY), 1.f};
	});
	for (uint32_t y = 0; y < 2; ++y) {
		for (uint32_t x = 0; x < 4; ++x) {
			const auto dir = Equirectangular::toDirection(
					{(static_cast<float>(x) + 0.5f) / 4.f, (static_cast<float>(y) + 0.5f) / 2.f});
			expectNear(Equirectangular::sample(image, dir), image.at(x, y), 1e-4f);
		}
	}
}

TEST(EnvironmentMap, ConstantSphericalHarmonics) {
	const math::vec3 radiance{0.5f, 1.f, 2.f};
	const auto sh = SphericalHarmonics::constant(radiance);
	for (const auto& dir: {math::vec3{0.f, 1.f, 0.f}, math::vec3{0.f, -1.f, 0.f}, math::vec3{1.f, 0.f, 0.f},
						   math::vec3{0.3f, -0.4f, 0.8f}.normalized()}) {
		expectNear(sh.evaluate(dir), radiance, 1e-5f);
	}
}

TEST(EnvironmentMap, ProjectedConstantSkyIsConstant) {
	const math::vec3 radiance{0.25f, 0.5f, 1.f};
	const auto env = EnvironmentMap::build(makeConstantImage(64, 32, radiance), 1);
	for (const auto& dir: {math::vec3{0.f, 1.f, 0.f}, math::vec3{0.f, -1.f, 0.f}, math::vec3{0.f, 0.f, -1.f},
						   math::vec3{0.6f, 0.2f, 0.7f}.normalized()}) {
		expectNear(env.irradiance.evaluate(dir), radiance, 1e-3f);
	}
}

TEST(EnvironmentMap, UpperHemisphereSkyLightsUpwards) {
	const auto image = makeImage(64, 32, [](uint32_t, const uint32_t iY) -> math::vec3 {
		return iY < 16 ? math::vec3{1.f, 1.f, 1.f} : math::vec3{0.f, 0.f, 0.f};
	});
	const auto env = EnvironmentMap::build(image, 1);
	const auto up = env.irradiance.evaluate({0.f, 1.f, 0.f});
	const auto down = env.irradiance.evaluate({0.f, -1.f, 0.f});
	EXPECT_GT(up.x(), down.x());
	EXPECT_GT(up.x(), 0.8f);
	EXPECT_LT(down.x(), 0.2f);
}

TEST(EnvironmentMap, SingleDirectionLobe) {
	// A bright patch around +X.
	const auto image = makeImage(64, 32, [](const uint32_t iX, const uint32_t iY) -> math::vec3 {
		return (iX >= 44 && iX < 52 && iY >= 12 && iY < 20) ? math::vec3{50.f, 50.f, 50.f} : math::vec3{0.f, 0.f, 0.f};
	});
	const auto env = EnvironmentMap::build(image, 1);
	EXPECT_GT(env.irradiance.evaluate({1.f, 0.f, 0.f}).x(), env.irradiance.evaluate({-1.f, 0.f, 0.f}).x());
	EXPECT_GT(env.irradiance.evaluate({1.f, 0.f, 0.f}).x(), env.irradiance.evaluate({0.f, 1.f, 0.f}).x());
}

TEST(EnvironmentMap, BuildLevelsOfConstantImage) {
	const math::vec3 radiance{2.f, 1.f, 0.5f};
	const auto env = EnvironmentMap::build(makeConstantImage(64, 32, radiance), 6, 16);
	// 64x32, 32x16, 16x8, then 8x4: a level narrower than 8 px is never added.
	ASSERT_EQ(env.levels.size(), 4u);
	EXPECT_EQ(env.levels[0].size, (math::vec2ui{64, 32}));
	EXPECT_EQ(env.levels[1].size, (math::vec2ui{32, 16}));
	EXPECT_EQ(env.levels[2].size, (math::vec2ui{16, 8}));
	EXPECT_EQ(env.levels[3].size, (math::vec2ui{8, 4}));
	for (const auto& level: env.levels) {
		ASSERT_EQ(level.pixels.size(), static_cast<size_t>(level.size.x()) * level.size.y());
		for (const auto& pixel: level.pixels) { expectNear(pixel, radiance, 1e-3f); }
	}
}

TEST(EnvironmentMap, BuildRespectsRequestedLevelCount) {
	const auto env = EnvironmentMap::build(makeConstantImage(256, 128, {1.f, 1.f, 1.f}), 3, 8);
	ASSERT_EQ(env.levels.size(), 3u);
	EXPECT_EQ(env.levels[2].size, (math::vec2ui{64, 32}));
}

TEST(EnvironmentMap, BuildDownsamplesWideInput) {
	const auto env = EnvironmentMap::build(makeConstantImage(4096, 2048, {1.f, 2.f, 3.f}), 1);
	ASSERT_EQ(env.levels.size(), 1u);
	EXPECT_EQ(env.levels[0].size, (math::vec2ui{renderer::g_maxEnvironmentWidth, renderer::g_maxEnvironmentWidth / 2}));
}

TEST(EnvironmentMap, BuildRejectsEmptyImage) {
	const auto env = EnvironmentMap::build(FloatImage{});
	EXPECT_TRUE(env.levels.empty());
}

TEST(EnvironmentMap, RoughLevelBlursAPatch) {
	const auto image = makeImage(64, 32, [](const uint32_t iX, const uint32_t iY) -> math::vec3 {
		return (iX >= 44 && iX < 52 && iY >= 12 && iY < 20) ? math::vec3{100.f, 100.f, 100.f}
															: math::vec3{0.f, 0.f, 0.f};
	});
	const auto env = EnvironmentMap::build(image, 4, 64);
	ASSERT_EQ(env.levels.size(), 4u);
	const auto sharpPeak = env.levels[0].at(47, 15).x();
	const auto roughPeak = env.levels[3].at(5, 1).x();
	EXPECT_GT(sharpPeak, roughPeak);
	EXPECT_GT(env.levels[3].at(5, 1).x(), 0.f);
}

TEST(EnvironmentMap, DfgTableRanges) {
	constexpr uint32_t size = 16;
	const auto table = renderer::computeDfgTable(size, 128);
	ASSERT_EQ(table.size(), static_cast<size_t>(size) * size);
	for (const auto& value: table) {
		EXPECT_GE(value.x(), 0.f);
		EXPECT_LE(value.x(), 1.01f);
		EXPECT_GE(value.y(), 0.f);
		EXPECT_LE(value.y(), 1.01f);
		EXPECT_LE(value.x() + value.y(), 1.05f);
		EXPECT_TRUE(std::isfinite(value.z()));
		EXPECT_GE(value.z(), 0.f);
	}
}

TEST(EnvironmentMap, DfgSmoothHeadOnIsFullyReflective) {
	constexpr uint32_t size = 16;
	const auto table = renderer::computeDfgTable(size, 256);
	const auto& value = table[size - 1];
	EXPECT_NEAR(value.x() + value.y(), 1.f, 0.05f);
}

TEST(EnvironmentMap, DfgGrazingAlbedoDropsWithRoughness) {
	constexpr uint32_t size = 16;
	const auto table = renderer::computeDfgTable(size, 256);
	const auto albedo = [&](const uint32_t iRow, const uint32_t iCol) -> float {
		const auto& value = table[static_cast<size_t>(iRow) * size + iCol];
		return value.x() + value.y();
	};
	EXPECT_GT(albedo(0, 1), albedo(size - 1, 1));
}

TEST(EnvironmentMap, ColorLutIdentity) {
	const auto lut = ColorLut::parse(identityCube());
	ASSERT_TRUE(lut.has_value());
	EXPECT_EQ(lut->size, 2u);
	EXPECT_EQ(lut->values.size(), 8u);
	for (const auto& color: {math::vec3{0.f, 0.f, 0.f}, math::vec3{1.f, 1.f, 1.f}, math::vec3{0.2f, 0.7f, 0.4f},
							 math::vec3{0.9f, 0.1f, 0.5f}}) {
		expectNear(lut->sample(color), color, 1e-5f);
	}
	expectNear(lut->sample({2.f, -1.f, 0.5f}), {1.f, 0.f, 0.5f}, 1e-5f);
}

TEST(EnvironmentMap, ColorLutDomainScaling) {
	const std::string text = "DOMAIN_MIN 0 0 0\nDOMAIN_MAX 2 2 2\n" + identityCube();
	const auto lut = ColorLut::parse(text);
	ASSERT_TRUE(lut.has_value());
	expectNear(lut->domainMax, {2.f, 2.f, 2.f}, 0.f);
	expectNear(lut->sample({1.f, 1.f, 1.f}), {0.5f, 0.5f, 0.5f}, 1e-5f);
	expectNear(lut->sample({2.f, 0.f, 1.f}), {1.f, 0.f, 0.5f}, 1e-5f);
}

TEST(EnvironmentMap, ColorLutRefusesMalformedText) {
	EXPECT_FALSE(ColorLut::parse("").has_value());
	EXPECT_FALSE(ColorLut::parse("0 0 0\n").has_value());
	EXPECT_FALSE(ColorLut::parse("LUT_1D_SIZE 2\n0 0 0\n1 1 1\n").has_value());
	EXPECT_FALSE(ColorLut::parse("LUT_3D_SIZE 2\n0 0 0\n1 1 1\n").has_value());
	EXPECT_FALSE(ColorLut::parse("LUT_3D_SIZE x\n").has_value());
	EXPECT_FALSE(ColorLut::parse("LUT_3D_SIZE 1\n0 0 0\n").has_value());
	auto text = identityCube();
	text += "0 0 0\n";
	EXPECT_FALSE(ColorLut::parse(text).has_value());
	text = identityCube();
	text.replace(text.rfind("1 1 1"), 5, "1 a 1");
	EXPECT_FALSE(ColorLut::parse(text).has_value());
}

TEST(EnvironmentMap, ColorLutStripLayout) {
	// Each entry encodes its own (r, g, b) so the strip position can be checked.
	std::string text = "LUT_3D_SIZE 2\n";
	for (int b = 0; b < 2; ++b) {
		for (int g = 0; g < 2; ++g) {
			for (int r = 0; r < 2; ++r) {
				text += std::to_string(r * 0.25 + 0.1) + " " + std::to_string(g * 0.25 + 0.2) + " " +
						std::to_string(b * 0.25 + 0.3) + "\n";
			}
		}
	}
	const auto lut = ColorLut::parse(text);
	ASSERT_TRUE(lut.has_value());
	const auto strip = lut->toStrip();
	ASSERT_EQ(strip.size(), 8u);
	constexpr size_t n = 2;
	for (size_t b = 0; b < n; ++b) {
		for (size_t g = 0; g < n; ++g) {
			for (size_t r = 0; r < n; ++r) {
				expectNear(strip[g * n * n + b * n + r], lut->values[r + g * n + b * n * n], 0.f);
				expectNear(strip[g * n * n + b * n + r],
						   {static_cast<float>(r) * 0.25f + 0.1f, static_cast<float>(g) * 0.25f + 0.2f,
							static_cast<float>(b) * 0.25f + 0.3f},
						   1e-5f);
			}
		}
	}
}

TEST(EnvironmentMap, ColorLutLoadFromFile) {
	const auto path = std::filesystem::temp_directory_path() / "owl_env_test_identity.cube";
	{
		std::ofstream out(path, std::ios::binary);
		out << identityCube();
	}
	const auto lut = ColorLut::load(path);
	std::filesystem::remove(path);
	ASSERT_TRUE(lut.has_value());
	EXPECT_EQ(lut->size, 2u);
	EXPECT_FALSE(ColorLut::load(path).has_value());
}

namespace {

auto tinyRadianceBytes() -> std::vector<uint8_t> {
	const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 4\n";
	std::vector<uint8_t> bytes(header.begin(), header.end());
	// RGBE (128, 64, 32, 129) decodes to (1.0, 0.5, 0.25); the second row is twice as bright.
	for (int row = 0; row < 2; ++row) {
		for (int x = 0; x < 4; ++x) { bytes.insert(bytes.end(), {128, 64, 32, static_cast<uint8_t>(129 + row)}); }
	}
	return bytes;
}

}// namespace

TEST(EnvironmentMap, DecodeHdrFromMemory) {
	const auto bytes = tinyRadianceBytes();
	const auto image = renderer::decodeHdrImage(bytes);
	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->size, (math::vec2ui{4, 2}));
	ASSERT_EQ(image->pixels.size(), 8u);
	expectNear(image->at(0, 0), {1.f, 0.5f, 0.25f}, 1e-6f);
	expectNear(image->at(3, 0), {1.f, 0.5f, 0.25f}, 1e-6f);
	expectNear(image->at(2, 1), {2.f, 1.f, 0.5f}, 1e-6f);
}

TEST(EnvironmentMap, LoadHdrFromFile) {
	const auto path = std::filesystem::temp_directory_path() / "owl_env_test_tiny.hdr";
	{
		const auto bytes = tinyRadianceBytes();
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	const auto image = renderer::loadHdrImage(path);
	std::filesystem::remove(path);
	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->size, (math::vec2ui{4, 2}));
	expectNear(image->at(1, 0), {1.f, 0.5f, 0.25f}, 1e-6f);
	expectNear(image->at(1, 1), {2.f, 1.f, 0.5f}, 1e-6f);
}

TEST(EnvironmentMap, LoadHdrMissingFile) {
	const auto path = std::filesystem::temp_directory_path() / "owl_env_test_missing.hdr";
	EXPECT_FALSE(renderer::loadHdrImage(path).has_value());
	EXPECT_FALSE(renderer::decodeHdrImage({}).has_value());
	const std::array<uint8_t, 4> garbage{1, 2, 3, 4};
	EXPECT_FALSE(renderer::decodeHdrImage(garbage).has_value());
}
