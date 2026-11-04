/**
 * @file elementaryFunctions_test.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/elementaryFunctions.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>

namespace math = owl::math;

namespace {

auto ulpDistance(const double iA, const double iB) -> uint64_t {
	const auto toOrdered = [](const double iValue) -> int64_t {
		const auto bits = std::bit_cast<int64_t>(iValue);
		return bits < 0 ? std::numeric_limits<int64_t>::min() - bits : bits;
	};
	const int64_t a = toOrdered(iA);
	const int64_t b = toOrdered(iB);
	return a > b ? static_cast<uint64_t>(a) - static_cast<uint64_t>(b)
				 : static_cast<uint64_t>(b) - static_cast<uint64_t>(a);
}

// Largest ulp error of a function against the libm over [iLow, iHigh].
auto maxUlp(const std::function<double(double)>& iOwn, const std::function<double(double)>& iReference,
			const double iLow, const double iHigh) -> uint64_t {
	constexpr int steps = 20011;
	uint64_t worst = 0;
	for (int step = 0; step <= steps; ++step) {
		const double x = iLow + (iHigh - iLow) * static_cast<double>(step) / steps;
		worst = std::max(worst, ulpDistance(iOwn(x), iReference(x)));
	}
	return worst;
}

// FNV-1a over the bits of a function on a fixed sweep, the fingerprint compared between targets.
auto fingerprint(const std::function<double(double)>& iFunction, const double iLow, const double iHigh) -> uint64_t {
	constexpr int steps = 4099;
	uint64_t hash = 0xcbf29ce484222325ull;
	for (int step = 0; step <= steps; ++step) {
		const double x = iLow + (iHigh - iLow) * static_cast<double>(step) / steps;
		hash = (hash ^ std::bit_cast<uint64_t>(iFunction(x))) * 0x100000001b3ull;
	}
	return hash;
}

}// namespace

TEST(ElementaryFunctions, StayWithinAFewUlpOfTheLibm) {
	constexpr uint64_t tolerance = 2;
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::sin(iX); },
					 [](const double iX) -> double { return std::sin(iX); }, -100.0, 100.0),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::cos(iX); },
					 [](const double iX) -> double { return std::cos(iX); }, -100.0, 100.0),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::tan(iX); },
					 [](const double iX) -> double { return std::tan(iX); }, -1.5, 1.5),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::atan(iX); },
					 [](const double iX) -> double { return std::atan(iX); }, -50.0, 50.0),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::asin(iX); },
					 [](const double iX) -> double { return std::asin(iX); }, -1.0, 1.0),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::acos(iX); },
					 [](const double iX) -> double { return std::acos(iX); }, -1.0, 1.0),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::exp(iX); },
					 [](const double iX) -> double { return std::exp(iX); }, -700.0, 700.0),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::log(iX); },
					 [](const double iX) -> double { return std::log(iX); }, 1e-6, 1e6),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::atan2(iX, 0.75); },
					 [](const double iX) -> double { return std::atan2(iX, 0.75); }, -10.0, 10.0),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::atan2(0.75, iX); },
					 [](const double iX) -> double { return std::atan2(0.75, iX); }, -10.0, 10.0),
			  tolerance);
	EXPECT_LE(maxUlp([](const double iX) -> double { return math::pow(iX, 2.5); },
					 [](const double iX) -> double { return std::pow(iX, 2.5); }, 0.0, 100.0),
			  64u);
}

TEST(ElementaryFunctions, GiveTheSameBitsOnEveryTarget) {
	EXPECT_EQ(fingerprint([](const double iX) -> double { return math::sin(iX); }, -1000.0, 1000.0),
			  0x4aeeab8012db2e76ull);
	EXPECT_EQ(fingerprint([](const double iX) -> double { return math::cos(iX); }, -1000.0, 1000.0),
			  0xb81abd35c3a32619ull);
	EXPECT_EQ(fingerprint([](const double iX) -> double { return math::tan(iX); }, -1000.0, 1000.0),
			  0x708bf842f764a551ull);
	EXPECT_EQ(fingerprint([](const double iX) -> double { return math::atan2(iX, -3.0); }, -1000.0, 1000.0),
			  0xc4ad7aafce751019ull);
	EXPECT_EQ(fingerprint([](const double iX) -> double { return math::asin(iX); }, -1.0, 1.0), 0x1b8e2cda6413c8b3ull);
	EXPECT_EQ(fingerprint([](const double iX) -> double { return math::exp(iX); }, -745.0, 709.0),
			  0xb19ea07f1fe8cd0aull);
	EXPECT_EQ(fingerprint([](const double iX) -> double { return math::log(iX); }, 1e-300, 1e300),
			  0x0b8d23d1dad73fd6ull);
	EXPECT_EQ(fingerprint([](const double iX) -> double { return math::pow(iX, -1.7); }, 1e-3, 1e3),
			  0x38c923e160b7e56full);
}

TEST(ElementaryFunctions, HandleTheSpecialValues) {
	constexpr double inf = std::numeric_limits<double>::infinity();
	EXPECT_TRUE(std::isnan(math::sin(inf)));
	EXPECT_TRUE(std::isnan(math::cos(std::numeric_limits<double>::quiet_NaN())));
	EXPECT_TRUE(std::isnan(math::asin(1.5)));
	EXPECT_TRUE(std::isnan(math::log(-1.0)));
	EXPECT_TRUE(std::isnan(math::pow(-2.0, 0.5)));
	EXPECT_EQ(math::log(0.0), -inf);
	EXPECT_EQ(math::exp(1000.0), inf);
	EXPECT_EQ(math::exp(-1000.0), 0.0);
	EXPECT_EQ(math::atan2(0.0, -1.0), std::numbers::pi);
	EXPECT_EQ(math::atan2(-1.0, 0.0), -std::numbers::pi / 2.0);
	EXPECT_EQ(math::pow(3.0, 2.0), 9.0);
	EXPECT_EQ(math::pow(-2.0, 3.0), -8.0);
	EXPECT_EQ(math::pow(2.0, -2.0), 0.25);
	EXPECT_EQ(math::pow(0.0, -1.0), inf);
	EXPECT_EQ(math::pow(5.0, 0.0), 1.0);
	EXPECT_EQ(math::exp(0.0), 1.0);
	EXPECT_EQ(math::log(1.0), 0.0);
	EXPECT_EQ(math::sin(0.0), 0.0);
	EXPECT_EQ(math::cos(0.0), 1.0);
}

TEST(ElementaryFunctions, FloatOverloadsRoundTheDoubleResult) {
	EXPECT_EQ(math::sin(0.5f), static_cast<float>(math::sin(0.5)));
	EXPECT_EQ(math::atan2(1.f, 2.f), static_cast<float>(math::atan2(1.0, 2.0)));
	EXPECT_EQ(math::pow(1.5f, 3.3f), static_cast<float>(math::pow(1.5, static_cast<double>(3.3f))));
	EXPECT_NEAR(math::cos(1.f), std::cos(1.f), 1e-6f);
	EXPECT_NEAR(math::exp(2.f), std::exp(2.f), 1e-5f);
	EXPECT_NEAR(math::log(3.f), std::log(3.f), 1e-6f);
	EXPECT_NEAR(math::tan(0.3f), std::tan(0.3f), 1e-6f);
	EXPECT_NEAR(math::acos(0.2f), std::acos(0.2f), 1e-6f);
	EXPECT_NEAR(math::atan(4.f), std::atan(4.f), 1e-6f);
}
