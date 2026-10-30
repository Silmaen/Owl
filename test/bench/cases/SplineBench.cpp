/**
 * @file SplineBench.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <math/SplineCurve.h>
#include <scene/component/Spline.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace owl::bench {

namespace {

// A closed wavy loop of 64 points, like a race track or a rail circuit.
auto makeTrack() -> std::vector<math::SplinePoint> {
	constexpr size_t count = 64;
	std::vector<math::SplinePoint> points;
	points.reserve(count);
	for (size_t index = 0; index < count; ++index) {
		const float angle = 6.2831853f * static_cast<float>(index) / static_cast<float>(count);
		const float radius = 50.f + 8.f * std::sin(angle * 5.f);
		points.push_back({.position = {radius * std::cos(angle), 2.f * std::sin(angle * 3.f), radius * std::sin(angle)},
						  .tangentIn = {},
						  .tangentOut = {}});
	}
	return points;
}

}// namespace

void runSplineBenches(Runner& ioRunner) {
	const auto points = makeTrack();
	scene::component::Spline spline;
	spline.curve.setClosed(true);
	spline.curve.setPoints(points);
	spline.lanes.push_back({.name = "outer", .offset = 2.f, .height = 0.f});
	ioRunner.measure("spline/rebuild/64_points", 1, [&]() -> void {
		spline.curve.setPoints(points);
		doNotOptimize(spline.curve.getLength());
	});
	const float length = spline.curve.getLength();
	constexpr size_t samples = 1000;
	ioRunner.measure("spline/sample_distance/1000", samples, [&]() -> void {
		float sum = 0.f;
		for (size_t index = 0; index < samples; ++index)
			sum += spline.curve.sampleAtDistance(length * static_cast<float>(index) / static_cast<float>(samples))
						   .position.x();
		doNotOptimize(sum);
	});
	ioRunner.measure("spline/sample_lane/1000", samples, [&]() -> void {
		float sum = 0.f;
		for (size_t index = 0; index < samples; ++index)
			sum += spline.sample(length * static_cast<float>(index) / static_cast<float>(samples), "outer").position.z();
		doNotOptimize(sum);
	});
	ioRunner.measure("spline/nearest/100", 100, [&]() -> void {
		float sum = 0.f;
		for (size_t index = 0; index < 100; ++index)
			sum += spline.curve.getNearestDistance({static_cast<float>(index) - 50.f, 0.f, 40.f});
		doNotOptimize(sum);
	});
}

}// namespace owl::bench
