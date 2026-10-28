/**
 * @file AreaLights_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/AreaLights.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::renderer;

namespace {

constexpr math::vec4 g_cosine{1.f, 0.f, 0.f, 1.f};

auto cellValue(const uint32_t iIndex) -> float {
	return static_cast<float>(iIndex) / static_cast<float>(g_ltcTableSize - 1);
}

auto storedTexel(const uint32_t iColumn, const uint32_t iRow) -> math::vec4 {
	return getLtcTable()[static_cast<size_t>(iRow) * g_ltcTableSize + iColumn];
}

// Projected solid angle of the part of a sphere above the horizon, by integration over its directions.
auto integrateClippedSphere(const double iElevationCos, const double iSinSigma) -> double {
	constexpr uint32_t steps = 1024;
	const double cosSigma = std::sqrt(1.0 - iSinSigma * iSinSigma);
	const double sinElevation = std::sqrt(1.0 - iElevationCos * iElevationCos);
	double sum = 0.0;
	for (uint32_t i = 0; i < steps; ++i) {
		const double cosTheta = 1.0 - (1.0 - cosSigma) * (static_cast<double>(i) + 0.5) / steps;
		const double sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);
		for (uint32_t j = 0; j < steps; ++j) {
			const double phi = 2.0 * std::numbers::pi * (static_cast<double>(j) + 0.5) / steps;
			const double z = cosTheta * iElevationCos + sinTheta * std::cos(phi) * sinElevation;
			sum += std::max(z, 0.0);
		}
	}
	return sum * 2.0 * std::numbers::pi * (1.0 - cosSigma) / (steps * steps);
}

auto hexFloat(const float iValue) -> std::string {
	return std::format("{}0x{:a}f", std::signbit(iValue) ? "-" : "", std::abs(iValue));
}

auto formatTable(const std::vector<math::vec4>& iTable) -> std::string {
	std::string source =
			"/**\n * @file LtcTable.cpp\n * @author Silmaen\n * @date 28/10/2026\n * Copyright (c) 2026 All "
			"rights reserved.\n * All modification must get authorization from the author.\n */\n\n"
			"#include \"owlpch.h\"\n\n#include \"renderer/AreaLights.h\"\n\n#include <array>\n"
			"#include <span>\n\nnamespace owl::renderer {\n\nnamespace {\n\n"
			"// NOLINTBEGIN(modernize-use-std-numbers) Fitted values, near a constant by chance.\n\n// clang-format "
			"off\n";
	source += std::format("constexpr std::array<math::vec4, {}> g_ltcTable{{{{\n", iTable.size());
	for (const auto& texel: iTable)
		source += std::format("\t\t{{{}, {}, {}, {}}},\n", hexFloat(texel.x()), hexFloat(texel.y()),
							  hexFloat(texel.z()), hexFloat(texel.w()));
	source += "}};\n// clang-format on\n\n// NOLINTEND(modernize-use-std-numbers)\n\n}// namespace\n\n"
			  "auto getLtcTable() -> std::span<const math::vec4> { return g_ltcTable; }\n\n"
			  "}// namespace owl::renderer\n";
	return source;
}

}// namespace

TEST(AreaLights, StoredTableHasEveryTexel) { EXPECT_EQ(getLtcTable().size(), g_ltcTableSize * g_ltcTableSize); }

TEST(AreaLights, StoredTableBeatsThePlainCosine) {
	for (const uint32_t column: {4u, 24u, 48u, 63u}) {
		for (const uint32_t row: {0u, 16u, 40u, 63u}) {
			const float roughness = cellValue(column);
			const float x = cellValue(row);
			const float cosTheta = 1.f - x * x;
			const auto texel = storedTexel(column, row);
			const float fitted = getLtcFitError(roughness, cosTheta, texel);
			EXPECT_TRUE(std::isfinite(fitted)) << column << ", " << row;
			EXPECT_LE(fitted, 0.5f * getLtcFitError(roughness, cosTheta, g_cosine)) << column << ", " << row;
		}
	}
}

TEST(AreaLights, NormalIncidenceIsIsotropic) {
	for (const uint32_t column: {0u, 20u, 63u}) {
		const auto texel = storedTexel(column, 0);
		EXPECT_NEAR(texel.y(), 0.f, 1e-6f);
		EXPECT_NEAR(texel.z(), 0.f, 1e-6f);
		EXPECT_GT(texel.x(), 0.f);
		EXPECT_GT(texel.w(), 0.f);
	}
}

TEST(AreaLights, RoughLobesTendToTheCosine) {
	const auto texel = storedTexel(g_ltcTableSize - 1, 0);
	EXPECT_NEAR(texel.x() / texel.w(), 1.f, 0.15f);
}

TEST(AreaLights, SmallFitMatchesTheStoredConvention) {
	const auto table = computeLtcTable(3);
	ASSERT_EQ(table.size(), 9u);
	for (const auto& texel: table) {
		EXPECT_TRUE(std::isfinite(texel.x()) && std::isfinite(texel.y()) && std::isfinite(texel.z()) &&
					std::isfinite(texel.w()));
	}
	EXPECT_NEAR(table[2].y(), 0.f, 1e-6f);
	EXPECT_NEAR(table[2].z(), 0.f, 1e-6f);
}

TEST(AreaLights, ClippedSphereTableMatchesTheIntegral) {
	const auto table = computeClippedSphereTable(g_ltcTableSize);
	ASSERT_EQ(table.size(), g_ltcTableSize * g_ltcTableSize);
	const auto at = [&](const uint32_t iColumn, const uint32_t iRow) -> float {
		return table[static_cast<size_t>(iRow) * g_ltcTableSize + iColumn];
	};
	// A sphere above the horizon is not clipped; one below it is gone.
	EXPECT_NEAR(at(g_ltcTableSize - 1, 10), 1.f, 1e-5f);
	EXPECT_NEAR(at(0, 10), 0.f, 1e-5f);
	EXPECT_NEAR(at(48, 0), std::max(2.f * cellValue(48) - 1.f, 0.f), 1e-6f);
	for (const uint32_t column: {20u, 31u, 40u}) {
		for (const uint32_t row: {8u, 32u, 60u}) {
			const double z = 2.0 * static_cast<double>(cellValue(column)) - 1.0;
			const auto formFactor = static_cast<double>(cellValue(row));
			const double expected = integrateClippedSphere(z, std::sqrt(formFactor)) / (std::numbers::pi * formFactor);
			EXPECT_NEAR(static_cast<double>(at(column, row)), expected, 2e-3) << column << ", " << row;
		}
	}
}

TEST(AreaLights, LuminanceSpreadsThePowerOverTheFace) {
	EXPECT_NEAR(getRectangleLuminance(std::numbers::pi_v<float>, {1.f, 1.f}), 1.f, 1e-6f);
	EXPECT_NEAR(getRectangleLuminance(std::numbers::pi_v<float> * 8.f, {2.f, 4.f}), 1.f, 1e-6f);
	EXPECT_NEAR(getDiscLuminance(std::numbers::pi_v<float> * std::numbers::pi_v<float>, 1.f), 1.f, 1e-5f);
	EXPECT_FLOAT_EQ(getRectangleLuminance(-5.f, {1.f, 1.f}), 0.f);
	EXPECT_TRUE(std::isfinite(getDiscLuminance(100.f, 0.f)));
}

// Rewrites source/owl/private/renderer/LtcTable.cpp when OWL_LTC_TABLE_UPDATE is set (the file lands next to the
// test binary).
TEST(AreaLights, WriteTheStoredTable) {
	if (std::getenv("OWL_LTC_TABLE_UPDATE") == nullptr)
		GTEST_SKIP() << "Set OWL_LTC_TABLE_UPDATE=1 to fit and write LtcTable.cpp.";
	const auto table = computeLtcTable(g_ltcTableSize);
	std::ofstream out{std::filesystem::current_path() / "LtcTable.cpp"};
	out << formatTable(table);
	EXPECT_TRUE(out.good());
}
