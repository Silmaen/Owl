/**
 * @file Particles_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/matrixCreation.h>
#include <renderer/LensFlare.h>
#include <renderer/ParticleSystem.h>
#include <renderer/RenderLayerFactory.h>
#include <renderer/Renderer.h>
#include <renderer/RendererParticles.h>
#include <renderer/RendererSet.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

using namespace owl;
using namespace owl::renderer;

namespace {

constexpr float g_frame = 1.f / 60.f;

auto still() -> ParticleEmitterSettings {
	ParticleEmitterSettings settings;
	settings.maxParticles = 64;
	settings.rate = 0.f;
	settings.shape = ParticleShape::Point;
	settings.speedMin = 0.f;
	settings.speedMax = 0.f;
	settings.lifetimeMin = 1.f;
	settings.lifetimeMax = 1.f;
	return settings;
}

auto at(const float iX, const float iY, const float iZ) -> math::mat4 {
	return math::translate(math::identity<float, 4>(), math::vec3{iX, iY, iZ});
}

auto alive(const ParticleSystem& iSystem) -> std::vector<GpuParticle> {
	std::vector<GpuParticle> result;
	for (const auto& particle: iSystem.getParticles())
		if (particle.isAlive())
			result.push_back(particle);
	return result;
}

}// namespace

TEST(ColorGradient, EvaluatesBetweenItsStopsAndHoldsTheEnds) {
	ColorGradient gradient;
	EXPECT_FLOAT_EQ(gradient.evaluate(0.3f).x(), 1.f);
	EXPECT_EQ(gradient.addKey({.time = 1.f, .color = {0.f, 0.f, 0.f, 0.f}}), 0u);
	EXPECT_EQ(gradient.addKey({.time = 0.f, .color = {1.f, 1.f, 1.f, 1.f}}), 0u);
	EXPECT_FLOAT_EQ(gradient.evaluate(0.25f).x(), 0.75f);
	EXPECT_FLOAT_EQ(gradient.evaluate(-1.f).w(), 1.f);
	EXPECT_FLOAT_EQ(gradient.evaluate(2.f).w(), 0.f);
}

TEST(ParticleTables, BakeTheTintTimesTheGradientAndTheSizeCurve) {
	ParticleEmitterSettings settings;
	settings.color = {0.5f, 1.f, 1.f, 1.f};
	settings.colorOverLife.addKey({.time = 0.f, .color = {1.f, 1.f, 1.f, 1.f}});
	settings.colorOverLife.addKey({.time = 1.f, .color = {1.f, 1.f, 1.f, 0.f}});
	settings.sizeOverLife.addKey({0.f, 2.f});
	settings.sizeOverLife.addKey({1.f, 0.f});
	const auto tables = ParticleTables::bake(settings);
	EXPECT_FLOAT_EQ(tables.color.front().x(), 0.5f);
	EXPECT_NEAR(tables.colorAt(0.5f).w(), 0.5f, 1e-5f);
	EXPECT_NEAR(tables.sizeAt(0.5f), 1.f, 1e-5f);
	EXPECT_FLOAT_EQ(tables.sizeAt(1.f), 0.f);
	const auto plain = ParticleTables::bake(ParticleEmitterSettings{});
	EXPECT_FLOAT_EQ(plain.sizeAt(0.7f), 1.f);
}

TEST(ParticleRandom, IsDeterministicAndInRange) {
	for (uint32_t serial = 0; serial < 1000; ++serial) {
		const float value = particleRandom(7, serial, 3);
		EXPECT_GE(value, 0.f);
		EXPECT_LT(value, 1.f);
		EXPECT_FLOAT_EQ(value, particleRandom(7, serial, 3));
	}
	EXPECT_NE(particleRandom(1, 5, 0), particleRandom(2, 5, 0));
}

TEST(ParticleSystem, SpawnsAtItsRateWhilePlaying) {
	auto settings = still();
	settings.rate = 60.f;
	ParticleSystem system;
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getSpawnedCount(), 0u);
	system.play();
	for (int frame = 0; frame < 30; ++frame) system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_NEAR(static_cast<double>(system.getSpawnedCount()), 30.0, 1.0);
	EXPECT_EQ(system.getAliveCount(), static_cast<uint32_t>(system.getSpawnedCount()));
}

TEST(ParticleSystem, ParticlesDieAtTheEndOfTheirLife) {
	auto settings = still();
	settings.burst = 10;
	ParticleSystem system;
	system.play();
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getAliveCount(), 10u);
	for (int frame = 0; frame < 59; ++frame) system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getAliveCount(), 10u);
	system.advance(settings, g_frame * 2.f, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getAliveCount(), 0u);
}

TEST(ParticleSystem, GravityAndDragMoveTheParticles) {
	auto settings = still();
	settings.burst = 1;
	settings.gravity = {0.f, -10.f, 0.f};
	ParticleSystem system;
	system.play();
	system.advance(settings, g_frame, at(0.f, 5.f, 0.f));
	for (int frame = 0; frame < 30; ++frame) system.advance(settings, g_frame, at(0.f, 5.f, 0.f));
	auto particles = alive(system);
	ASSERT_EQ(particles.size(), 1u);
	EXPECT_NEAR(particles[0].position.y(), 5.f - 0.5f * 10.f * 0.25f, 0.1f);
	EXPECT_NEAR(particles[0].velocity.y(), -5.f, 1e-3f);
	settings.drag = 100.f;
	system.restart();
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	for (int frame = 0; frame < 30; ++frame) system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	particles = alive(system);
	ASSERT_EQ(particles.size(), 1u);
	EXPECT_GT(particles[0].velocity.y(), -0.2f);
}

TEST(ParticleSystem, TheRingReplacesTheOldestPastItsCapacity) {
	auto settings = still();
	settings.maxParticles = 8;
	settings.lifetimeMin = settings.lifetimeMax = 100.f;
	ParticleSystem system;
	system.play();
	system.emit(5);
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	system.emit(5);
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getCapacity(), 8u);
	EXPECT_EQ(system.getAliveCount(), 8u);
	EXPECT_EQ(system.getSpawnedCount(), 10u);
	const auto& particles = system.getParticles();
	EXPECT_FLOAT_EQ(particles[0].position.w(), 0.f);
	EXPECT_FLOAT_EQ(particles[4].position.w(), g_frame);
}

TEST(ParticleSystem, DurationStopsOrLoopsTheEmission) {
	auto settings = still();
	settings.rate = 10.f;
	settings.burst = 4;
	settings.duration = 0.5f;
	settings.loop = false;
	ParticleSystem once;
	once.play();
	for (int frame = 0; frame < 120; ++frame) once.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_FALSE(once.isEmitting());
	EXPECT_NEAR(static_cast<double>(once.getSpawnedCount()), 9.0, 1.0);
	settings.loop = true;
	ParticleSystem looping;
	looping.play();
	for (int frame = 0; frame < 120; ++frame) looping.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_TRUE(looping.isEmitting());
	EXPECT_NEAR(static_cast<double>(looping.getSpawnedCount()), 4.0 * 4.0 + 20.0, 2.0);
}

TEST(ParticleSystem, PauseFreezesStopEndsTheEmissionRestartClears) {
	auto settings = still();
	settings.rate = 60.f;
	ParticleSystem system;
	system.play();
	for (int frame = 0; frame < 10; ++frame) system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	const auto spawned = system.getSpawnedCount();
	system.pause();
	EXPECT_FALSE(system.isPlaying());
	for (int frame = 0; frame < 10; ++frame) system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getSpawnedCount(), spawned);
	system.play();
	system.stop();
	for (int frame = 0; frame < 10; ++frame) system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getSpawnedCount(), spawned);
	EXPECT_GT(system.getAliveCount(), 0u);
	system.restart();
	EXPECT_EQ(system.getAliveCount(), 0u);
	EXPECT_TRUE(system.isEmitting());
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getSpawnedCount(), 1u);
}

TEST(ParticleSystem, WorldParticlesStayLocalOnesFollowTheEmitter) {
	auto settings = still();
	settings.burst = 1;
	settings.lifetimeMin = settings.lifetimeMax = 10.f;
	ParticleSystem world;
	world.play();
	world.advance(settings, g_frame, at(1.f, 0.f, 0.f));
	world.advance(settings, g_frame, at(5.f, 0.f, 0.f));
	EXPECT_FLOAT_EQ(alive(world).front().position.x(), 1.f);
	settings.space = ParticleSpace::Local;
	ParticleSystem local;
	local.play();
	local.advance(settings, g_frame, at(1.f, 0.f, 0.f));
	local.advance(settings, g_frame, at(5.f, 0.f, 0.f));
	EXPECT_FLOAT_EQ(alive(local).front().position.x(), 0.f);
	EXPECT_FLOAT_EQ(local.getEmitterMatrix()(0, 3), 5.f);
}

TEST(ParticleSystem, SpawnsOfAFrameSpreadAlongTheMoveOfTheEmitter) {
	auto settings = still();
	ParticleSystem system;
	system.play();
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	system.emit(4);
	system.advance(settings, g_frame, at(4.f, 0.f, 0.f));
	auto particles = alive(system);
	ASSERT_EQ(particles.size(), 4u);
	std::ranges::sort(particles, {}, [](const GpuParticle& iParticle) -> float { return iParticle.position.x(); });
	EXPECT_FLOAT_EQ(particles.front().position.x(), 0.5f);
	EXPECT_FLOAT_EQ(particles.back().position.x(), 3.5f);
}

TEST(ParticleSystem, FollowsAShiftOfTheFloatingOrigin) {
	auto settings = still();
	settings.burst = 1;
	settings.trailLength = 3;
	settings.lifetimeMin = settings.lifetimeMax = 10.f;
	ParticleSystem system;
	system.play();
	system.advance(settings, g_frame, at(100.f, 0.f, 0.f));
	system.shiftOrigin({100.f, 0.f, 0.f});
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_NEAR(alive(system).front().position.x(), 0.f, 1e-4f);
	for (const auto& point: system.getTrails()) EXPECT_NEAR(point.x(), 0.f, 1e-4f);
	system.pause();
	system.shiftOrigin({-10.f, 0.f, 0.f});
	system.advance(settings, g_frame, at(10.f, 0.f, 0.f));
	EXPECT_NEAR(alive(system).front().position.x(), 10.f, 1e-4f);
}

TEST(ParticleSystem, TrailsRecordAPointEachInterval) {
	auto settings = still();
	settings.burst = 1;
	settings.speedMin = settings.speedMax = 1.f;
	settings.shape = ParticleShape::Cone;
	settings.coneAngle = 0.f;
	settings.shapeSize = {0.f, 0.f, 0.f};
	settings.trailLength = 4;
	settings.trailInterval = 0.1f;
	settings.lifetimeMin = settings.lifetimeMax = 10.f;
	ParticleSystem system;
	system.play();
	for (int frame = 0; frame < 36; ++frame) system.advance(settings, 0.01f, at(0.f, 0.f, 0.f));
	ASSERT_EQ(system.getTrailLength(), 4u);
	const auto trail = system.getTrails().subspan(0, 4);
	EXPECT_NEAR(trail[0].y(), 0.3f, 0.02f);
	EXPECT_NEAR(trail[1].y(), 0.2f, 0.02f);
	EXPECT_NEAR(trail[2].y(), 0.1f, 0.02f);
	EXPECT_NEAR(trail[3].y(), 0.f, 1e-5f);
}

TEST(ParticleSystem, AlphaBlendingCapsTheCapacityToTheSort) {
	ParticleEmitterSettings settings;
	settings.maxParticles = 5000;
	settings.blend = ParticleBlend::Alpha;
	settings.trailLength = 100;
	ParticleSystem system;
	system.play();
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.getCapacity(), g_maxSortedParticles);
	EXPECT_EQ(system.getTrailLength(), g_maxParticleTrail);
}

TEST(ParticleSystem, TheGpuPathRecordsTheStepsUntilTheyAreTaken) {
	auto settings = still();
	settings.rate = 60.f;
	ParticleSystem system{ParticleSystem::Path::Gpu};
	system.play();
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_TRUE(system.getParticles().empty());
	ASSERT_TRUE(system.hasPendingStep());
	const auto& step = system.takeStep();
	EXPECT_FALSE(system.hasPendingStep());
	EXPECT_NEAR(step.velocity.w(), 2.f * g_frame, 1e-6f);
	EXPECT_EQ(step.spawn[0], 0u);
	EXPECT_EQ(step.spawn[1], 2u);
	EXPECT_EQ(step.spawn[3], 64u);
	EXPECT_EQ(step.flags[0], 1u);
	system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
	EXPECT_EQ(system.takeStep().flags[0], 0u);
}

TEST(ParticleSystem, SimulateParticleKillsEverySlotOnAReset) {
	ParticleStepParams params;
	params.flags = {1, 0, 0, 0};
	params.spawn = {0, 0, 0, 4};
	GpuParticle particle;
	particle.position = {0.f, 0.f, 0.f, 0.f};
	particle.velocity = {0.f, 0.f, 0.f, 5.f};
	simulateParticle(particle, {}, 2, params);
	EXPECT_FALSE(particle.isAlive());
}

TEST(ParticleSystem, EveryShapeSpawnsInsideItsVolume) {
	for (const auto shape: {ParticleShape::Point, ParticleShape::Sphere, ParticleShape::Box, ParticleShape::Cone,
							ParticleShape::Circle}) {
		auto settings = still();
		settings.shape = shape;
		settings.shapeSize = {1.f, 1.f, 1.f};
		settings.speedMin = settings.speedMax = 1.f;
		settings.burst = 50;
		ParticleSystem system;
		system.play();
		system.advance(settings, g_frame, at(0.f, 0.f, 0.f));
		for (const auto& particle: alive(system)) {
			const math::vec3 position{particle.position.x(), particle.position.y(), particle.position.z()};
			const math::vec3 velocity{particle.velocity.x(), particle.velocity.y(), particle.velocity.z()};
			EXPECT_LE(position.norm(), 1.75f);
			EXPECT_NEAR(velocity.norm(), 1.f, 1e-4f);
			if (shape == ParticleShape::Circle)
				EXPECT_FLOAT_EQ(position.z(), 0.f);
			if (shape == ParticleShape::Cone || shape == ParticleShape::Box)
				EXPECT_GT(velocity.y(), 0.f);
		}
	}
}

TEST(ParticlePreset, EveryPresetIsAValidEmitter) {
	for (const auto preset: {ParticlePreset::Fire, ParticlePreset::Smoke, ParticlePreset::Sparks, ParticlePreset::Magic,
							 ParticlePreset::Snow}) {
		const auto settings = makeParticlePreset(preset);
		EXPECT_GT(settings.rate, 0.f);
		EXPECT_LE(settings.lifetimeMin, settings.lifetimeMax);
		EXPECT_LE(settings.trailLength, g_maxParticleTrail);
		if (settings.blend == ParticleBlend::Alpha)
			EXPECT_LE(settings.maxParticles, g_maxSortedParticles);
	}
	EXPECT_EQ(makeParticlePreset(ParticlePreset::Smoke).blend, ParticleBlend::Alpha);
	EXPECT_GT(makeParticlePreset(ParticlePreset::Sparks).trailLength, 0u);
}

TEST(LensFlare, GhostsCrossTheCentre) {
	EXPECT_FLOAT_EQ(LensFlare::ghostOffset(0, 0.5f), 0.5f);
	EXPECT_FLOAT_EQ(LensFlare::ghostOffset(1, 0.5f), 0.f);
	EXPECT_FLOAT_EQ(LensFlare::ghostOffset(3, 0.5f), -1.f);
}

TEST(LensFlare, BrightnessFollowsTheIlluminanceAndClamps) {
	EXPECT_FLOAT_EQ(LensFlare::brightnessOf(100.f, 10.f, 1.f, 1.f), 1.f);
	EXPECT_FLOAT_EQ(LensFlare::brightnessOf(100.f, 20.f, 1.f, 1.f), 0.25f);
	EXPECT_FLOAT_EQ(LensFlare::brightnessOf(100.f, 10.f, 1.f, 0.f), 0.f);
	EXPECT_FLOAT_EQ(LensFlare::brightnessOf(1.e5f, 0.f, 1.f, 1.f), LensFlare::g_maxBrightness);
	EXPECT_FLOAT_EQ(LensFlare::brightnessOf(1.e5f, 0.f, 2.5e-5f, 2.f), 5.f);
}

TEST(LensFlare, NothingIsRecordedWithoutFlaresOrSources) {
	RenderGraph graph;
	const auto color = graph.createTarget("colour", RenderGraph::TargetFormat::Linear);
	const auto depth = graph.createTarget("depth", RenderGraph::TargetFormat::Depth);
	PostProcessSettings settings;
	const LensFlareSource source{.position = {0.f, 0.f, -5.f}, .directional = false, .color = {1.f, 1.f, 1.f}};
	const std::array sources{source};
	EXPECT_FALSE(
			LensFlare::recordLensFlares(graph, color, depth, settings, math::identity<float, 4>(), sources, {64, 64})
					.isValid());
	settings.lensFlare = true;
	EXPECT_FALSE(LensFlare::recordLensFlares(graph, color, depth, settings, math::identity<float, 4>(), {}, {64, 64})
						 .isValid());
}

TEST(RendererParticles, TheNullBackendSimulatesOnTheCpuAndRegistersTheLayer) {
	core::Log::init(core::Log::Level::Off);
	RenderLayerFactory::clear();
	gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
	Renderer::initContext();
	Renderer::initShaders({}, RendererSet::base());
	EXPECT_TRUE(RenderLayerFactory::hasType("RendererParticles"));
	const auto layer = RenderLayerFactory::create("RendererParticles", "fx");
	ASSERT_NE(layer, nullptr);
	EXPECT_EQ(layer->getName(), "fx");
	EXPECT_FALSE(RendererParticles::isReady());
	const auto system = RendererParticles::createSystem();
	EXPECT_EQ(system->getPath(), ParticleSystem::Path::Cpu);
	const auto folders = RendererSet::base().getShaderFolders();
	EXPECT_NE(std::ranges::find(folders, "particles"), folders.end());
	EXPECT_NE(std::ranges::find(folders, "lens_flare"), folders.end());
	EXPECT_NE(std::ranges::find(folders, "bitonic_sort"), folders.end());
	Renderer::shutdown();
	RenderLayerFactory::clear();
	core::Log::invalidate();
}
