/**
 * @file Orbit.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "math/Orbit.h"
#include "math/elementaryFunctions.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace owl::math {

namespace {

// Beyond this argument exp overflows a double: the hyperbola is then out of reach.
constexpr double g_MaxHyperbolicArgument = 700.0;
constexpr int g_MaxNewtonIterations = 64;
constexpr double g_NewtonTolerance = 1e-12;

struct Stumpff {
	double c;
	double s;
};

// Stumpff functions C(z) and S(z) of the universal variable formulation.
auto stumpff(const double iZ) -> std::optional<Stumpff> {
	if (iZ > 1e-8) {
		const double root = std::sqrt(iZ);
		return Stumpff{.c = (1.0 - math::cos(root)) / iZ, .s = (root - math::sin(root)) / (iZ * root)};
	}
	if (iZ < -1e-8) {
		const double root = std::sqrt(-iZ);
		if (root > g_MaxHyperbolicArgument)
			return std::nullopt;
		const double grow = math::exp(root);
		const double shrink = 1.0 / grow;
		const double cosh = 0.5 * (grow + shrink);
		const double sinh = 0.5 * (grow - shrink);
		return Stumpff{.c = (cosh - 1.0) / -iZ, .s = (sinh - root) / (-iZ * root)};
	}
	// Taylor series near the parabola.
	return Stumpff{.c = 0.5 - iZ / 24.0 + iZ * iZ / 720.0, .s = 1.0 / 6.0 - iZ / 120.0 + iZ * iZ / 5040.0};
}

auto straightLine(const OrbitState& iState, const double iSeconds) -> OrbitState {
	return {.position = iState.position + iState.velocity * iSeconds, .velocity = iState.velocity};
}

auto isFinite(const vec3d& iVector) -> bool {
	return std::isfinite(iVector.x()) && std::isfinite(iVector.y()) && std::isfinite(iVector.z());
}

}// namespace

auto propagateKepler(const OrbitState& iState, const double iGravitationalParameter, double iSeconds) -> OrbitState {
	const double r0 = iState.position.norm();
	if (iGravitationalParameter <= 0.0 || r0 <= 0.0 || iSeconds == 0.0)
		return straightLine(iState, iSeconds);
	const double mu = iGravitationalParameter;
	const double sqrtMu = std::sqrt(mu);
	const double radialSpeed = (iState.position * iState.velocity) / r0;
	// Inverse of the semi-major axis: positive on an ellipse, zero on a parabola, negative on a hyperbola.
	const double alpha = 2.0 / r0 - iState.velocity.normSq() / mu;
	if (alpha > 1e-12) {
		// Whole periods bring the body back to its start: only the rest of the duration is solved.
		const double period = 2.0 * std::numbers::pi * std::sqrt(1.0 / (alpha * alpha * alpha) / mu);
		iSeconds = std::fmod(iSeconds, period);
	}
	double chi = alpha > 1e-12 ? sqrtMu * alpha * iSeconds : sqrtMu * iSeconds / r0;
	const double radialTerm = r0 * radialSpeed / sqrtMu;
	const double energyTerm = 1.0 - alpha * r0;
	Stumpff functions{.c = 0.5, .s = 1.0 / 6.0};
	bool converged = false;
	for (int iteration = 0; iteration < g_MaxNewtonIterations; ++iteration) {
		const double chiSq = chi * chi;
		const auto value = stumpff(alpha * chiSq);
		if (!value)
			return straightLine(iState, iSeconds);
		functions = *value;
		const double timeError = radialTerm * chiSq * functions.c + energyTerm * chiSq * chi * functions.s + r0 * chi -
								 sqrtMu * iSeconds;
		const double slope =
				radialTerm * chi * (1.0 - alpha * chiSq * functions.s) + energyTerm * chiSq * functions.c + r0;
		if (slope == 0.0 || !std::isfinite(slope))
			break;
		const double delta = timeError / slope;
		chi -= delta;
		if (std::abs(delta) <= g_NewtonTolerance * std::max(1.0, std::abs(chi))) {
			converged = true;
			break;
		}
	}
	const auto final = stumpff(alpha * chi * chi);
	if (!converged || !final)
		return straightLine(iState, iSeconds);
	functions = *final;
	const double chiSq = chi * chi;
	const double f = 1.0 - chiSq / r0 * functions.c;
	const double g = iSeconds - chiSq * chi / sqrtMu * functions.s;
	const vec3d position = iState.position * f + iState.velocity * g;
	const double r = position.norm();
	if (r <= 0.0)
		return straightLine(iState, iSeconds);
	const double fDot = sqrtMu / (r * r0) * (alpha * chiSq * chi * functions.s - chi);
	const double gDot = 1.0 - chiSq / r * functions.c;
	const OrbitState result{.position = position, .velocity = iState.position * fDot + iState.velocity * gDot};
	return isFinite(result.position) && isFinite(result.velocity) ? result : straightLine(iState, iSeconds);
}

auto propagateUniform(const OrbitState& iState, const vec3d& iAcceleration, const double iSeconds) -> OrbitState {
	return {.position = iState.position + iState.velocity * iSeconds + iAcceleration * (0.5 * iSeconds * iSeconds),
			.velocity = iState.velocity + iAcceleration * iSeconds};
}

}// namespace owl::math
