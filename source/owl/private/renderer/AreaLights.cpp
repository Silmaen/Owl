/**
 * @file AreaLights.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/AreaLights.h"

#include "renderer/utils/ParallelRows.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numbers>
#include <ranges>
#include <span>
#include <vector>

namespace owl::renderer {

namespace {

constexpr double g_pi = std::numbers::pi;
constexpr double g_minAlpha = 1e-4;
constexpr uint32_t g_fitSamples = 32;
constexpr uint32_t g_averageSamples = 64;
constexpr uint32_t g_maxIterations = 100;

using Vec = std::array<double, 3>;
// Row-major 3x3 matrix.
using Mat = std::array<double, 9>;

auto dot(const Vec& iA, const Vec& iB) -> double { return iA[0] * iB[0] + iA[1] * iB[1] + iA[2] * iB[2]; }

auto normalize(const Vec& iV) -> Vec {
	const double length = std::sqrt(dot(iV, iV));
	return {iV[0] / length, iV[1] / length, iV[2] / length};
}

auto mul(const Mat& iM, const Vec& iV) -> Vec {
	return {iM[0] * iV[0] + iM[1] * iV[1] + iM[2] * iV[2], iM[3] * iV[0] + iM[4] * iV[1] + iM[5] * iV[2],
			iM[6] * iV[0] + iM[7] * iV[1] + iM[8] * iV[2]};
}

auto mul(const Mat& iA, const Mat& iB) -> Mat {
	Mat result{};
	for (size_t row = 0; row < 3; ++row)
		for (size_t col = 0; col < 3; ++col)
			result[row * 3 + col] =
					iA[row * 3] * iB[col] + iA[row * 3 + 1] * iB[3 + col] + iA[row * 3 + 2] * iB[6 + col];
	return result;
}

auto determinant(const Mat& iM) -> double {
	return iM[0] * (iM[4] * iM[8] - iM[5] * iM[7]) - iM[1] * (iM[3] * iM[8] - iM[5] * iM[6]) +
		   iM[2] * (iM[3] * iM[7] - iM[4] * iM[6]);
}

auto inverse(const Mat& iM) -> Mat {
	const double inv = 1.0 / determinant(iM);
	return {(iM[4] * iM[8] - iM[5] * iM[7]) * inv, (iM[2] * iM[7] - iM[1] * iM[8]) * inv,
			(iM[1] * iM[5] - iM[2] * iM[4]) * inv, (iM[5] * iM[6] - iM[3] * iM[8]) * inv,
			(iM[0] * iM[8] - iM[2] * iM[6]) * inv, (iM[2] * iM[3] - iM[0] * iM[5]) * inv,
			(iM[3] * iM[7] - iM[4] * iM[6]) * inv, (iM[1] * iM[6] - iM[0] * iM[7]) * inv,
			(iM[0] * iM[4] - iM[1] * iM[3]) * inv};
}

// GGX lobe with the height-correlated Smith visibility, cosine of the light included, no Fresnel.
struct Ggx {
	double alpha;
	Vec view;

	[[nodiscard]] auto lambda(const double iCos) const -> double {
		if (iCos >= 1.0)
			return 0.0;
		const double tan2 = (1.0 - iCos * iCos) / (iCos * iCos);
		return 0.5 * (-1.0 + std::sqrt(1.0 + alpha * alpha * tan2));
	}

	[[nodiscard]] auto evaluate(const Vec& iLight, double& oPdf) const -> double {
		oPdf = 0.0;
		if (view[2] <= 0.0)
			return 0.0;
		const Vec half = normalize({view[0] + iLight[0], view[1] + iLight[1], view[2] + iLight[2]});
		const double a2 = alpha * alpha;
		const double f = half[2] * half[2] * (a2 - 1.0) + 1.0;
		const double distribution = a2 / (g_pi * f * f);
		oPdf = std::abs(distribution * half[2] / (4.0 * dot(view, half)));
		if (iLight[2] <= 0.0)
			return 0.0;
		const double shadowing = 1.0 / (1.0 + lambda(view[2]) + lambda(iLight[2]));
		return distribution * shadowing / (4.0 * view[2]);
	}

	[[nodiscard]] auto sample(const double iU1, const double iU2) const -> Vec {
		const double phi = 2.0 * g_pi * iU1;
		const double radius = alpha * std::sqrt(iU2 / (1.0 - iU2));
		const Vec normal = normalize({radius * std::cos(phi), radius * std::sin(phi), 1.0});
		const double projection = 2.0 * dot(normal, view);
		return {normal[0] * projection - view[0], normal[1] * projection - view[1], normal[2] * projection - view[2]};
	}
};

// A clamped cosine distribution seen through a linear transform M.
struct Ltc {
	Mat transform;
	Mat inverseTransform;
	double inverseDeterminant;
	double magnitude;

	static auto fromTransform(const Mat& iTransform, const double iMagnitude) -> Ltc {
		const Mat inv = inverse(iTransform);
		return {.transform = iTransform,
				.inverseTransform = inv,
				.inverseDeterminant = std::abs(determinant(inv)),
				.magnitude = iMagnitude};
	}

	[[nodiscard]] auto evaluate(const Vec& iLight) const -> double {
		const Vec original = mul(inverseTransform, iLight);
		const double length = std::sqrt(dot(original, original));
		const double cosine = std::max(0.0, original[2] / length);
		return magnitude * cosine / g_pi * inverseDeterminant / (length * length * length);
	}

	[[nodiscard]] auto sample(const double iU1, const double iU2) const -> Vec {
		const double sinTheta = std::sqrt(iU1);
		const double phi = 2.0 * g_pi * iU2;
		return normalize(mul(transform, Vec{sinTheta * std::cos(phi), sinTheta * std::sin(phi), std::sqrt(1.0 - iU1)}));
	}
};

// Mean of the cubed difference of the two lobes, sampled from both of them.
auto fitError(const Ltc& iLtc, const Ggx& iGgx) -> double {
	double error = 0.0;
	const auto accumulate = [&](const Vec& iLight) -> void {
		double brdfPdf = 0.0;
		const double brdf = iGgx.evaluate(iLight, brdfPdf);
		const double ltc = iLtc.evaluate(iLight);
		const double ltcPdf = ltc / iLtc.magnitude;
		const double difference = std::abs(brdf - ltc);
		const double term = difference * difference * difference / (ltcPdf + brdfPdf);
		if (std::isfinite(term))
			error += term;
	};
	for (uint32_t j = 0; j < g_fitSamples; ++j) {
		for (uint32_t k = 0; k < g_fitSamples; ++k) {
			const double u1 = (static_cast<double>(j) + 0.5) / g_fitSamples;
			const double u2 = (static_cast<double>(k) + 0.5) / g_fitSamples;
			accumulate(iLtc.sample(u1, u2));
			accumulate(iGgx.sample(u1, u2));
		}
	}
	return error / static_cast<double>(g_fitSamples * g_fitSamples);
}

struct LobeAverage {
	double magnitude = 0.0;
	Vec direction{0.0, 0.0, 1.0};
};

// Integral of the lobe over the sphere and its mean light direction, in the plane of the view.
auto averageLobe(const Ggx& iGgx) -> LobeAverage {
	LobeAverage result;
	Vec direction{0.0, 0.0, 0.0};
	for (uint32_t j = 0; j < g_averageSamples; ++j) {
		for (uint32_t k = 0; k < g_averageSamples; ++k) {
			const Vec light = iGgx.sample((static_cast<double>(j) + 0.5) / g_averageSamples,
										  (static_cast<double>(k) + 0.5) / g_averageSamples);
			double pdf = 0.0;
			const double value = iGgx.evaluate(light, pdf);
			if (pdf <= 0.0)
				continue;
			const double weight = value / pdf;
			result.magnitude += weight;
			for (size_t axis = 0; axis < 3; ++axis) direction[axis] += weight * light[axis];
		}
	}
	result.magnitude /= static_cast<double>(g_averageSamples * g_averageSamples);
	direction[1] = 0.0;
	if (dot(direction, direction) > 0.0)
		result.direction = normalize(direction);
	return result;
}

// Free parameters of a fitted cosine: its scale along X and Y and its shear of X along Z.
struct LtcParameters {
	double m11 = 1.0;
	double m22 = 1.0;
	double m13 = 0.0;
};

auto buildLtc(const Mat& iFrame, const LtcParameters& iParameters, const double iMagnitude) -> Ltc {
	const Mat scale{iParameters.m11, 0.0, iParameters.m13, 0.0, iParameters.m22, 0.0, 0.0, 0.0, 1.0};
	return Ltc::fromTransform(mul(iFrame, scale), iMagnitude);
}

template<typename Fn>
auto nelderMead(const std::vector<double>& iStart, Fn&& iFunction) -> std::vector<double> {
	const size_t dimension = iStart.size();
	std::vector<std::vector<double>> simplex(dimension + 1, iStart);
	for (size_t axis = 0; axis < dimension; ++axis) simplex[axis + 1][axis] += 0.05;
	std::vector<double> values(dimension + 1);
	for (size_t vertex = 0; vertex <= dimension; ++vertex) values[vertex] = iFunction(simplex[vertex]);
	const auto blend = [&](const std::vector<double>& iFrom, const std::vector<double>& iTo,
						   const double iFactor) -> std::vector<double> {
		std::vector<double> point(dimension);
		for (size_t axis = 0; axis < dimension; ++axis) point[axis] = iFrom[axis] + (iTo[axis] - iFrom[axis]) * iFactor;
		return point;
	};
	for (uint32_t iteration = 0; iteration < g_maxIterations; ++iteration) {
		std::vector<size_t> order(dimension + 1);
		for (size_t vertex = 0; vertex <= dimension; ++vertex) order[vertex] = vertex;
		std::ranges::sort(order, [&](const size_t iA, const size_t iB) -> bool { return values[iA] < values[iB]; });
		const size_t best = order.front();
		const size_t worst = order.back();
		if (values[worst] - values[best] <= 1e-6 * std::max(std::abs(values[best]), 1e-12))
			break;
		std::vector<double> centroid(dimension, 0.0);
		for (size_t vertex = 0; vertex < dimension; ++vertex)
			for (size_t axis = 0; axis < dimension; ++axis)
				centroid[axis] += simplex[order[vertex]][axis] / static_cast<double>(dimension);
		const auto reflected = blend(centroid, simplex[worst], -1.0);
		const double reflectedValue = iFunction(reflected);
		if (reflectedValue < values[best]) {
			const auto expanded = blend(centroid, simplex[worst], -2.0);
			const double expandedValue = iFunction(expanded);
			const bool expand = expandedValue < reflectedValue;
			simplex[worst] = expand ? expanded : reflected;
			values[worst] = expand ? expandedValue : reflectedValue;
			continue;
		}
		if (reflectedValue < values[order[dimension - 1]]) {
			simplex[worst] = reflected;
			values[worst] = reflectedValue;
			continue;
		}
		const auto contracted = blend(centroid, simplex[worst], 0.5);
		if (const double contractedValue = iFunction(contracted); contractedValue < values[worst]) {
			simplex[worst] = contracted;
			values[worst] = contractedValue;
			continue;
		}
		for (size_t vertex = 0; vertex <= dimension; ++vertex) {
			if (vertex == best)
				continue;
			simplex[vertex] = blend(simplex[best], simplex[vertex], 0.5);
			values[vertex] = iFunction(simplex[vertex]);
		}
	}
	const auto bestValue = std::ranges::min_element(values);
	return simplex[static_cast<size_t>(std::distance(values.begin(), bestValue))];
}

auto lobeOf(const double iRoughness, const double iCosTheta) -> Ggx {
	const double cosTheta = std::clamp(iCosTheta, std::cos(1.57), 1.0);
	return {.alpha = std::max(iRoughness * iRoughness, g_minAlpha),
			.view = {std::sqrt(1.0 - cosTheta * cosTheta), 0.0, cosTheta}};
}

struct FittedCell {
	LtcParameters parameters;
	math::vec4 texel;
};

// Fit one texel from a starting guess; at normal incidence the lobe is isotropic and keeps the identity frame.
auto fitCell(const uint32_t iSize, const uint32_t iColumn, const uint32_t iRow, const LtcParameters& iStart)
		-> FittedCell {
	const double step = 1.0 / static_cast<double>(iSize - 1);
	const double x = static_cast<double>(iRow) * step;
	const Ggx ggx = lobeOf(static_cast<double>(iColumn) * step, 1.0 - x * x);
	const LobeAverage average = averageLobe(ggx);
	const bool isotropic = iRow == 0;
	Mat frame{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
	if (!isotropic) {
		const Vec& z = average.direction;
		frame = {z[2], 0.0, z[0], 0.0, 1.0, 0.0, -z[0], 0.0, z[2]};
	}
	const auto toParameters = [&](const std::vector<double>& iValues) -> LtcParameters {
		if (isotropic)
			return {.m11 = std::max(iValues[0], g_minAlpha), .m22 = std::max(iValues[0], g_minAlpha), .m13 = 0.0};
		return {.m11 = std::max(iValues[0], g_minAlpha), .m22 = std::max(iValues[1], g_minAlpha), .m13 = iValues[2]};
	};
	const auto error = [&](const std::vector<double>& iValues) -> double {
		return fitError(buildLtc(frame, toParameters(iValues), average.magnitude), ggx);
	};
	const std::vector<double> start =
			isotropic ? std::vector{iStart.m11} : std::vector{iStart.m11, iStart.m22, iStart.m13};
	const LtcParameters fitted = toParameters(nelderMead(start, error));
	const Mat inv = buildLtc(frame, fitted, average.magnitude).inverseTransform;
	const double norm = inv[4];
	return {.parameters = fitted,
			.texel = {static_cast<float>(inv[0] / norm), static_cast<float>(inv[6] / norm),
					  static_cast<float>(inv[2] / norm), static_cast<float>(inv[8] / norm)}};
}

// Projected solid angle above the horizon of a sphere of radius iSigma, centre iOmega off the normal (Snyder 1996).
auto clippedSphere(const double iOmega, const double iSigma) -> double {
	const double sinSigma2 = std::sin(iSigma) * std::sin(iSigma);
	if (iOmega <= g_pi / 2.0 - iSigma)
		return g_pi * std::cos(iOmega) * sinSigma2;
	if (iOmega >= g_pi / 2.0 + iSigma)
		return 0.0;
	const double sinOmega = std::sin(iOmega);
	const double gamma = std::asin(std::clamp(std::cos(iSigma) / sinOmega, -1.0, 1.0));
	const double cosGamma = std::cos(gamma);
	const double g = -2.0 * sinOmega * std::cos(iSigma) * cosGamma + g_pi / 2.0 - gamma + std::sin(gamma) * cosGamma;
	const double h = std::cos(iOmega) * (cosGamma * std::sqrt(std::max(sinSigma2 - cosGamma * cosGamma, 0.0)) +
										 sinSigma2 * std::asin(std::clamp(cosGamma / std::sin(iSigma), -1.0, 1.0)));
	if (iOmega < g_pi / 2.0)
		return g_pi * std::cos(iOmega) * sinSigma2 + g - h;
	return g + h;
}

}// namespace

auto computeLtcTable(const uint32_t iSize) -> std::vector<math::vec4> {
	OWL_PROFILE_FUNCTION()

	const uint32_t size = std::max(2u, iSize);
	std::vector<math::vec4> table(static_cast<size_t>(size) * size);
	std::vector<LtcParameters> normalIncidence(size);
	LtcParameters start;
	for (const uint32_t column: std::views::iota(0u, size) | std::views::reverse) {
		const auto cell = fitCell(size, column, 0, start);
		normalIncidence[column] = cell.parameters;
		table[column] = cell.texel;
		start = cell.parameters;
	}
	utils::parallelRows(size, [&](const uint32_t iColumn) -> void {
		LtcParameters previous = normalIncidence[iColumn];
		for (uint32_t row = 1; row < size; ++row) {
			const auto cell = fitCell(size, iColumn, row, previous);
			table[static_cast<size_t>(row) * size + iColumn] = cell.texel;
			previous = cell.parameters;
		}
	});
	return table;
}

auto getLtcFitError(const float iRoughness, const float iCosTheta, const math::vec4& iInverse) -> float {
	const Ggx ggx = lobeOf(static_cast<double>(iRoughness), static_cast<double>(iCosTheta));
	const Mat inv{static_cast<double>(iInverse.x()), 0.0, static_cast<double>(iInverse.z()), 0.0, 1.0, 0.0,
				  static_cast<double>(iInverse.y()), 0.0, static_cast<double>(iInverse.w())};
	const Ltc ltc = Ltc::fromTransform(inverse(inv), averageLobe(ggx).magnitude);
	return static_cast<float>(fitError(ltc, ggx));
}

auto computeClippedSphereTable(const uint32_t iSize) -> std::vector<float> {
	const uint32_t size = std::max(2u, iSize);
	const double step = 1.0 / static_cast<double>(size - 1);
	std::vector<float> table(static_cast<size_t>(size) * size);
	for (uint32_t row = 0; row < size; ++row) {
		const double formFactor = static_cast<double>(row) * step;
		const double sigma = std::asin(std::sqrt(formFactor));
		for (uint32_t column = 0; column < size; ++column) {
			const double z = 2.0 * static_cast<double>(column) * step - 1.0;
			const double value =
					formFactor > 0.0 ? clippedSphere(std::acos(z), sigma) / (g_pi * formFactor) : std::max(z, 0.0);
			table[static_cast<size_t>(row) * size + column] = static_cast<float>(value);
		}
	}
	return table;
}

auto getRectangleLuminance(const float iPower, const math::vec2& iSize) -> float {
	const float area = std::max(iSize.x() * iSize.y(), 1e-6f);
	return std::max(iPower, 0.f) / (std::numbers::pi_v<float> * area);
}

auto getDiscLuminance(const float iPower, const float iRadius) -> float {
	const float area = std::max(std::numbers::pi_v<float> * iRadius * iRadius, 1e-6f);
	return std::max(iPower, 0.f) / (std::numbers::pi_v<float> * area);
}

}// namespace owl::renderer
