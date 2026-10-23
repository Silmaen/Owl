/**
 * @file SkyBench.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <renderer/EnvironmentMap.h>
#include <renderer/ProceduralSky.h>

#include <cstdint>
#include <format>

namespace owl::bench {

void runSkyBenches(Runner& ioRunner) {
	const renderer::SkyParameters dusk{.sunDirection = math::vec3{0.8f, 0.05f, -0.6f}.normalized()};
	for (const uint32_t width: {256U, 512U, 1024U}) {
		const math::vec2ui size{width, width / 2};
		ioRunner.measure(std::format("sky/draw/{}", width), uint64_t{width} * (width / 2),
						 [&]() -> void { doNotOptimize(renderer::renderProceduralSky(dusk, size)); });
		const auto image = renderer::renderProceduralSky(dusk, size);
		ioRunner.measure(std::format("sky/prefilter/{}", width), uint64_t{width} * (width / 2),
						 [&]() -> void { doNotOptimize(renderer::EnvironmentMap::build(image)); });
	}
}

}// namespace owl::bench
