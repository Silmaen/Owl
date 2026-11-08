/**
 * @file Orbit_test.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/Orbit.h>

#include <cmath>
#include <numbers>

using namespace owl::math;

namespace {

constexpr double g_Mu = 4000.0;

auto energy(const OrbitState& iState, const double iMu) -> double {
	return 0.5 * iState.velocity.normSq() - iMu / iState.position.norm();
}

auto circular(const double iRadius) -> OrbitState {
	return {.position = {iRadius, 0.0, 0.0}, .velocity = {0.0, std::sqrt(g_Mu / iRadius), 0.0}};
}

auto period(const double iSemiMajorAxis) -> double {
	return 2.0 * std::numbers::pi * std::sqrt(iSemiMajorAxis * iSemiMajorAxis * iSemiMajorAxis / g_Mu);
}

}// namespace

TEST(Orbit, CircularOrbitTurnsAQuarterInAQuarterPeriod) {
	const auto state = propagateKepler(circular(100.0), g_Mu, period(100.0) * 0.25);
	EXPECT_NEAR(state.position.x(), 0.0, 1e-6);
	EXPECT_NEAR(state.position.y(), 100.0, 1e-6);
	EXPECT_NEAR(state.velocity.x(), -std::sqrt(g_Mu / 100.0), 1e-8);
	EXPECT_NEAR(state.velocity.y(), 0.0, 1e-8);
}

TEST(Orbit, EllipseComesBackAfterManyPeriods) {
	const OrbitState start{.position = {80.0, 10.0, 5.0}, .velocity = {-1.5, 7.0, 0.5}};
	const double alpha = 2.0 / start.position.norm() - start.velocity.normSq() / g_Mu;
	ASSERT_GT(alpha, 0.0);
	const auto state = propagateKepler(start, g_Mu, period(1.0 / alpha) * 1000.0);
	EXPECT_NEAR(state.position.x(), start.position.x(), 1e-5);
	EXPECT_NEAR(state.position.y(), start.position.y(), 1e-5);
	EXPECT_NEAR(state.position.z(), start.position.z(), 1e-5);
	EXPECT_NEAR(energy(state, g_Mu), energy(start, g_Mu), 1e-9);
}

TEST(Orbit, HyperbolaKeepsItsEnergyAndAngularMomentum) {
	const OrbitState start{.position = {50.0, 0.0, 0.0}, .velocity = {2.0, 20.0, 0.0}};
	ASSERT_GT(energy(start, g_Mu), 0.0);
	const auto state = propagateKepler(start, g_Mu, 30.0);
	EXPECT_NEAR(energy(state, g_Mu), energy(start, g_Mu), 1e-9);
	const auto momentum = [](const OrbitState& iState) -> double {
		return iState.position.x() * iState.velocity.y() - iState.position.y() * iState.velocity.x();
	};
	EXPECT_NEAR(momentum(state), momentum(start), 1e-8);
	EXPECT_GT(state.position.norm(), start.position.norm());
}

TEST(Orbit, GoingBackInTimeReturnsToTheStart) {
	const OrbitState start{.position = {60.0, 20.0, 0.0}, .velocity = {-3.0, 6.0, 1.0}};
	const auto back = propagateKepler(propagateKepler(start, g_Mu, 12.5), g_Mu, -12.5);
	EXPECT_NEAR(back.position.x(), start.position.x(), 1e-7);
	EXPECT_NEAR(back.position.y(), start.position.y(), 1e-7);
	EXPECT_NEAR(back.velocity.z(), start.velocity.z(), 1e-9);
}

TEST(Orbit, WithoutAttractorTheBodyGoesStraight) {
	const OrbitState start{.position = {1.0, 2.0, 3.0}, .velocity = {1.0, 0.0, -1.0}};
	const auto state = propagateKepler(start, 0.0, 2.0);
	EXPECT_DOUBLE_EQ(state.position.x(), 3.0);
	EXPECT_DOUBLE_EQ(state.position.z(), 1.0);
	const auto centred = propagateKepler({.position = {0.0, 0.0, 0.0}, .velocity = {1.0, 0.0, 0.0}}, g_Mu, 1.0);
	EXPECT_DOUBLE_EQ(centred.position.x(), 1.0);
}

TEST(Orbit, UniformPullDrawsAParabola) {
	const auto state =
			propagateUniform({.position = {0.0, 0.0, 0.0}, .velocity = {2.0, 10.0, 0.0}}, {0.0, -10.0, 0.0}, 2.0);
	EXPECT_DOUBLE_EQ(state.position.x(), 4.0);
	EXPECT_DOUBLE_EQ(state.position.y(), 0.0);
	EXPECT_DOUBLE_EQ(state.velocity.y(), -10.0);
}

TEST(Orbit, SameInputsGiveTheSameBits) {
	const OrbitState start{.position = {70.0, -15.0, 3.0}, .velocity = {2.5, 6.5, -0.25}};
	const auto first = propagateKepler(start, g_Mu, 123.456);
	const auto second = propagateKepler(start, g_Mu, 123.456);
	EXPECT_EQ(first.position.x(), second.position.x());
	EXPECT_EQ(first.velocity.y(), second.velocity.y());
}
