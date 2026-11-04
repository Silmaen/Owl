/**
 * @file elementaryFunctions.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "math/elementaryFunctions.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

// Polynomials and reductions of fdlibm (Sun Microsystems, freely distributable).
namespace owl::math {

namespace {

constexpr double g_Pi = std::numbers::pi;
constexpr double g_PiLow = 1.22464679914735317720e-16;
// ln 2 split so that k * g_Ln2High is exact, not the rounded std::numbers::ln2.
constexpr double g_Ln2High = 6.93147180369123816490e-01;// NOLINT(modernize-use-std-numbers)
constexpr double g_Ln2Low = 1.90821492927058770002e-10;
constexpr double g_HalfPi = 1.57079632679489655800e+00;
constexpr double g_Nan = std::numeric_limits<double>::quiet_NaN();
constexpr double g_Inf = std::numeric_limits<double>::infinity();

auto kernelSin(const double iX, const double iTail) -> double {
	constexpr double s1 = -1.66666666666666324348e-01;
	constexpr double s2 = 8.33333333332248946124e-03;
	constexpr double s3 = -1.98412698298579493134e-04;
	constexpr double s4 = 2.75573137070700676789e-06;
	constexpr double s5 = -2.50507602534068634195e-08;
	constexpr double s6 = 1.58969099521155010221e-10;
	const double z = iX * iX;
	const double v = z * iX;
	const double r = s2 + z * (s3 + z * (s4 + z * (s5 + z * s6)));
	return iX - ((z * (0.5 * iTail - v * r) - iTail) - v * s1);
}

auto kernelCos(const double iX, const double iTail) -> double {
	constexpr double c1 = 4.16666666666666019037e-02;
	constexpr double c2 = -1.38888888888741095749e-03;
	constexpr double c3 = 2.48015872894767294178e-05;
	constexpr double c4 = -2.75573143513906633035e-07;
	constexpr double c5 = 2.08757232129817482790e-09;
	constexpr double c6 = -1.13596475577881948265e-11;
	const double z = iX * iX;
	const double r = z * (c1 + z * (c2 + z * (c3 + z * (c4 + z * (c5 + z * c6)))));
	const double hz = 0.5 * z;
	const double w = 1.0 - hz;
	return w + (((1.0 - w) - hz) + (z * r - iX * iTail));
}

// Angle reduced to [-pi/4, pi/4] as head + tail, and its quadrant.
struct Reduced {
	double head;
	double tail;
	int quadrant;
};

auto reduce(const double iX) -> Reduced {
	// pi/2 split in 33-bit parts: fn * part is exact while |fn| < 2^20.
	constexpr double invHalfPi = 6.36619772367581382433e-01;
	constexpr double part1 = 1.57079632673412561417e+00;
	constexpr double part2 = 6.07710050630396597660e-11;
	constexpr double part2Tail = 2.02226624879595063154e-21;
	constexpr double part3 = 2.02226624871116645580e-21;
	constexpr double part3Tail = 8.47842766036889956997e-32;
	const double fn = std::floor(iX * invHalfPi + 0.5);
	double r = iX - fn * part1;
	double t = r;
	double w = fn * part2;
	r = t - w;
	w = fn * part2Tail - ((t - r) - w);
	t = r;
	w = fn * part3;
	r = t - w;
	w = fn * part3Tail - ((t - r) - w);
	const double head = r - w;
	const auto quadrant = static_cast<int>(std::fmod(fn, 4.0));
	return {.head = head, .tail = (r - head) - w, .quadrant = quadrant < 0 ? quadrant + 4 : quadrant};
}

auto isOddInteger(const double iY) -> bool { return std::floor(iY) == iY && std::fmod(iY, 2.0) != 0.0; }

auto integerPower(double iX, const double iY) -> double {
	auto exponent = static_cast<uint32_t>(std::fabs(iY));
	double result = 1.0;
	while (exponent != 0) {
		if ((exponent & 1u) != 0)
			result *= iX;
		iX *= iX;
		exponent >>= 1u;
	}
	return iY < 0.0 ? 1.0 / result : result;
}

}// namespace

auto sin(const double iX) -> double {
	if (!std::isfinite(iX))
		return g_Nan;
	const auto [head, tail, quadrant] = reduce(iX);
	switch (quadrant) {
		case 0:
			return kernelSin(head, tail);
		case 1:
			return kernelCos(head, tail);
		case 2:
			return -kernelSin(head, tail);
		default:
			return -kernelCos(head, tail);
	}
}

auto cos(const double iX) -> double {
	if (!std::isfinite(iX))
		return g_Nan;
	const auto [head, tail, quadrant] = reduce(iX);
	switch (quadrant) {
		case 0:
			return kernelCos(head, tail);
		case 1:
			return -kernelSin(head, tail);
		case 2:
			return -kernelCos(head, tail);
		default:
			return kernelSin(head, tail);
	}
}

auto tan(const double iX) -> double {
	if (!std::isfinite(iX))
		return g_Nan;
	const auto [head, tail, quadrant] = reduce(iX);
	const double sine = kernelSin(head, tail);
	const double cosine = kernelCos(head, tail);
	return (quadrant & 1) == 0 ? sine / cosine : -cosine / sine;
}

auto atan(const double iX) -> double {
	constexpr std::array<double, 4> atanHigh{4.63647609000806093515e-01, 7.85398163397448278999e-01,
											 9.82793723247329054082e-01, 1.57079632679489655800e+00};
	constexpr std::array<double, 4> atanLow{2.26987774529616870924e-17, 3.06161699786838301793e-17,
											1.39033110312309984516e-17, 6.12323399573676603587e-17};
	constexpr std::array<double, 11> coefficients{
			3.33333333333329318027e-01,  -1.99999999998764832476e-01, 1.42857142725034663711e-01,
			-1.11111104054623557880e-01, 9.09088713343650656196e-02,  -7.69187620504482999495e-02,
			6.66107313738753120669e-02,  -5.83357013379057348645e-02, 4.97687799461593236017e-02,
			-3.65315727442169155270e-02, 1.62858201153657823623e-02};
	if (std::isnan(iX))
		return iX + iX;
	double x = std::fabs(iX);
	if (x >= 0x1.0p66)
		return std::copysign(atanHigh[3] + atanLow[3], iX);
	if (x < 0x1.0p-27)
		return iX;
	int index = -1;
	if (x >= 2.4375) {
		index = 3;
		x = -1.0 / x;
	} else if (x >= 1.1875) {
		index = 2;
		x = (x - 1.5) / (1.0 + 1.5 * x);
	} else if (x >= 0.6875) {
		index = 1;
		x = (x - 1.0) / (x + 1.0);
	} else if (x >= 0.4375) {
		index = 0;
		x = (2.0 * x - 1.0) / (2.0 + x);
	}
	const double z = x * x;
	const double w = z * z;
	const double even =
			z * (coefficients[0] +
				 w * (coefficients[2] +
					  w * (coefficients[4] + w * (coefficients[6] + w * (coefficients[8] + w * coefficients[10])))));
	const double odd =
			w * (coefficients[1] +
				 w * (coefficients[3] + w * (coefficients[5] + w * (coefficients[7] + w * coefficients[9]))));
	if (index < 0)
		return std::copysign(x - x * (even + odd), iX);
	const auto slot = static_cast<size_t>(index);
	return std::copysign(atanHigh[slot] - ((x * (even + odd) - atanLow[slot]) - x), iX);
}

auto atan2(const double iY, const double iX) -> double {
	if (std::isnan(iX) || std::isnan(iY))
		return iX + iY;
	if (iY == 0.0)
		return std::signbit(iX) ? std::copysign(g_Pi, iY) : iY;
	if (iX == 0.0)
		return std::copysign(g_HalfPi, iY);
	if (std::isinf(iX)) {
		if (std::isinf(iY))
			return std::copysign(std::signbit(iX) ? 3.0 * g_Pi / 4.0 : g_Pi / 4.0, iY);
		return std::copysign(std::signbit(iX) ? g_Pi : 0.0, iY);
	}
	if (std::isinf(iY))
		return std::copysign(g_HalfPi, iY);
	const double angle = atan(std::fabs(iY / iX));
	return std::copysign(std::signbit(iX) ? g_Pi - (angle - g_PiLow) : angle, iY);
}

auto asin(const double iX) -> double {
	if (std::fabs(iX) > 1.0)
		return g_Nan;
	return atan2(iX, std::sqrt((1.0 - iX) * (1.0 + iX)));
}

auto acos(const double iX) -> double {
	if (std::fabs(iX) > 1.0)
		return g_Nan;
	return atan2(std::sqrt((1.0 - iX) * (1.0 + iX)), iX);
}

auto exp(const double iX) -> double {
	constexpr double overflow = 7.09782712893383973096e+02;
	constexpr double underflow = -7.45133219101941108420e+02;
	constexpr double invLn2 = std::numbers::log2e;
	constexpr double p1 = 1.66666666666666019037e-01;
	constexpr double p2 = -2.77777777770155933842e-03;
	constexpr double p3 = 6.61375632143793436117e-05;
	constexpr double p4 = -1.65339022054652515390e-06;
	constexpr double p5 = 4.13813679705723846039e-08;
	if (std::isnan(iX))
		return iX + iX;
	if (iX > overflow)
		return g_Inf;
	if (iX < underflow)
		return 0.0;
	const double k = std::floor(iX * invLn2 + 0.5);
	const double high = iX - k * g_Ln2High;
	const double low = k * g_Ln2Low;
	const double r = high - low;
	const double t = r * r;
	const double c = r - t * (p1 + t * (p2 + t * (p3 + t * (p4 + t * p5))));
	const double y = 1.0 - ((low - (r * c) / (2.0 - c)) - high);
	return std::ldexp(y, static_cast<int>(k));
}

auto log(const double iX) -> double {
	constexpr double lg1 = 6.666666666666735130e-01;
	constexpr double lg2 = 3.999999999940941908e-01;
	constexpr double lg3 = 2.857142874366239149e-01;
	constexpr double lg4 = 2.222219843214978396e-01;
	constexpr double lg5 = 1.818357216161805012e-01;
	constexpr double lg6 = 1.531383769920937332e-01;
	constexpr double lg7 = 1.479819860511658591e-01;
	constexpr double halfSqrt2 = 7.07106781186547524401e-01;
	if (std::isnan(iX) || (std::isinf(iX) && iX > 0.0))
		return iX + iX;
	if (iX < 0.0)
		return g_Nan;
	if (iX == 0.0)
		return -g_Inf;
	int exponent = 0;
	double mantissa = std::frexp(iX, &exponent);
	if (mantissa < halfSqrt2) {
		mantissa *= 2.0;
		--exponent;
	}
	const double f = mantissa - 1.0;
	const double s = f / (2.0 + f);
	const auto dk = static_cast<double>(exponent);
	const double z = s * s;
	const double w = z * z;
	const double r = z * (lg1 + w * (lg3 + w * (lg5 + w * lg7))) + w * (lg2 + w * (lg4 + w * lg6));
	const double hfsq = 0.5 * f * f;
	return dk * g_Ln2High - ((hfsq - (s * (hfsq + r) + dk * g_Ln2Low)) - f);
}

auto pow(const double iX, const double iY) -> double {
	if (iY == 0.0 || iX == 1.0)
		return 1.0;
	if (std::isnan(iX) || std::isnan(iY))
		return iX + iY;
	if (std::floor(iY) == iY && std::fabs(iY) <= 64.0)
		return integerPower(iX, iY);
	const bool negative = std::signbit(iX);
	if (negative && std::isfinite(iX) && iX != 0.0 && std::floor(iY) != iY)
		return g_Nan;
	const double magnitude = exp(iY * log(std::fabs(iX)));
	return negative && isOddInteger(iY) ? -magnitude : magnitude;
}

auto sin(const float iX) -> float { return static_cast<float>(sin(static_cast<double>(iX))); }

auto cos(const float iX) -> float { return static_cast<float>(cos(static_cast<double>(iX))); }

auto tan(const float iX) -> float { return static_cast<float>(tan(static_cast<double>(iX))); }

auto asin(const float iX) -> float { return static_cast<float>(asin(static_cast<double>(iX))); }

auto acos(const float iX) -> float { return static_cast<float>(acos(static_cast<double>(iX))); }

auto atan(const float iX) -> float { return static_cast<float>(atan(static_cast<double>(iX))); }

auto atan2(const float iY, const float iX) -> float {
	return static_cast<float>(atan2(static_cast<double>(iY), static_cast<double>(iX)));
}

auto exp(const float iX) -> float { return static_cast<float>(exp(static_cast<double>(iX))); }

auto log(const float iX) -> float { return static_cast<float>(log(static_cast<double>(iX))); }

auto pow(const float iX, const float iY) -> float {
	return static_cast<float>(pow(static_cast<double>(iX), static_cast<double>(iY)));
}

}// namespace owl::math
