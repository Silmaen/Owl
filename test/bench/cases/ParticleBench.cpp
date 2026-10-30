/**
 * @file ParticleBench.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <math/matrixCreation.h>
#include <renderer/ParticleSystem.h>

#include <cstdint>
#include <format>
#include <vector>

namespace owl::bench {

void runParticleBenches(Runner& ioRunner) {
	constexpr float frame = 1.f / 60.f;
	const math::mat4 emitter = math::identity<float, 4>();
	for (const uint32_t count: {1000U, 10000U, 65536U}) {
		auto settings = renderer::makeParticlePreset(renderer::ParticlePreset::Fire);
		settings.maxParticles = count;
		settings.rate = static_cast<float>(count);
		settings.lifetimeMin = settings.lifetimeMax = 1.f;
		renderer::ParticleSystem system;
		system.play();
		// Filled first: every slot lives while it is measured.
		for (int warm = 0; warm < 70; ++warm) system.advance(settings, frame, emitter);
		ioRunner.measure(std::format("particles/simulate_cpu/{}", count), count, [&]() -> void {
			system.advance(settings, frame, emitter);
			doNotOptimize(system.getSpawnedCount());
		});
	}
	auto sparks = renderer::makeParticlePreset(renderer::ParticlePreset::Sparks);
	std::vector<uniq<renderer::ParticleSystem>> systems;
	systems.reserve(256);
	for (int index = 0; index < 256; ++index) {
		systems.push_back(mkUniq<renderer::ParticleSystem>(renderer::ParticleSystem::Path::Gpu));
		systems.back()->play();
	}
	ioRunner.measure("particles/advance_gpu/256_emitters", 256, [&]() -> void {
		for (const auto& system: systems) {
			system->advance(sparks, frame, emitter);
			doNotOptimize(system->takeStep());
		}
	});
}

}// namespace owl::bench
