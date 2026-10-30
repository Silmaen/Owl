/**
 * @file ParticleEmitter_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <scene/systems/EngineSystems.h>

#include <chrono>
#include <cmath>

using namespace owl;
using namespace owl::scene;

namespace {

class ParticleEmitterTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }

	static void run(Scene& ioScene, const int iFrames) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(16));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	static auto makeEmitter(Scene& ioScene, const bool iPlayOnStart) -> Entity {
		auto entity = ioScene.createEntity("Emitter");
		auto& emitter = entity.addComponent<component::ParticleEmitter>();
		emitter.settings.rate = 50.f;
		emitter.settings.lifetimeMin = emitter.settings.lifetimeMax = 10.f;
		emitter.playOnStart = iPlayOnStart;
		return entity;
	}
};

}// namespace

TEST_F(ParticleEmitterTest, PlayStartsTheEmittersThatPlayOnStart) {
	auto scene = mkShared<Scene>();
	const auto playing = makeEmitter(*scene, true);
	const auto waiting = makeEmitter(*scene, false);
	scene->onStartRuntime();
	run(*scene, 30);
	const auto& played = playing.getComponent<component::ParticleEmitter>().m_slot.system;
	const auto& idle = waiting.getComponent<component::ParticleEmitter>().m_slot.system;
	ASSERT_NE(played, nullptr);
	ASSERT_NE(idle, nullptr);
	EXPECT_GT(played->getAliveCount(), 10u);
	EXPECT_EQ(idle->getAliveCount(), 0u);
	scene->onEndRuntime();
}

TEST_F(ParticleEmitterTest, TheEditorPreviewPlaysEveryEmitter) {
	auto scene = mkShared<Scene>();
	const auto waiting = makeEmitter(*scene, false);
	for (int frame = 0; frame < 10; ++frame) systems::advanceEffects(*scene, 0.016f, true);
	const auto& system = waiting.getComponent<component::ParticleEmitter>().m_slot.system;
	ASSERT_NE(system, nullptr);
	EXPECT_TRUE(system->isPlaying());
	EXPECT_GT(system->getSpawnedCount(), 0u);
}

TEST_F(ParticleEmitterTest, ACopyOfTheSceneStartsWithItsOwnSystems) {
	auto scene = mkShared<Scene>();
	const auto entity = makeEmitter(*scene, true);
	systems::advanceEffects(*scene, 0.016f, true);
	ASSERT_NE(entity.getComponent<component::ParticleEmitter>().m_slot.system, nullptr);
	const auto copy = Scene::copy(scene);
	const auto copied = copy->findEntityByUUID(entity.getUUID());
	ASSERT_TRUE(copied);
	EXPECT_EQ(copied.getComponent<component::ParticleEmitter>().m_slot.system, nullptr);
}

TEST_F(ParticleEmitterTest, ATrailRecordsTheMovesAndForgetsTheOldPoints) {
	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Comet");
	auto& trail = entity.addComponent<component::Trail>();
	trail.lifetime = 0.1f;
	trail.minDistance = 0.5f;
	auto& transform = entity.getComponent<component::Transform>().transform;
	for (int frame = 0; frame < 5; ++frame) {
		transform.translation() = math::vec3{static_cast<float>(frame), 0.f, 0.f};
		systems::advanceEffects(*scene, 0.01f, false);
	}
	auto& points = entity.getComponent<component::Trail>().m_points;
	ASSERT_EQ(points.size(), 6u);
	EXPECT_FLOAT_EQ(points.front().x(), 4.f);
	EXPECT_FLOAT_EQ(points.back().x(), 0.f);
	for (int frame = 0; frame < 30; ++frame) systems::advanceEffects(*scene, 0.01f, false);
	EXPECT_EQ(points.size(), 2u);
	entity.getComponent<component::Trail>().enabled = false;
	systems::advanceEffects(*scene, 0.01f, false);
	EXPECT_TRUE(points.empty());
}

TEST_F(ParticleEmitterTest, TrailWidthAndColourDefaultToATaper) {
	const component::Trail trail;
	const auto tables = trail.bakeTables();
	EXPECT_FLOAT_EQ(tables.sizeAt(0.f), 1.f);
	EXPECT_FLOAT_EQ(tables.sizeAt(1.f), 0.f);
	EXPECT_FLOAT_EQ(tables.colorAt(0.f).w(), 1.f);
	EXPECT_FLOAT_EQ(tables.colorAt(1.f).w(), 0.f);
}

TEST_F(ParticleEmitterTest, ParticlesAndTrailsFollowTheFloatingOrigin) {
	auto scene = mkShared<Scene>();
	auto emitter = makeEmitter(*scene, true);
	emitter.getComponent<component::Transform>().transform.translation() = math::vec3{1000.f, 0.f, 0.f};
	auto& trail = emitter.addComponent<component::Trail>();
	trail.lifetime = 100.f;
	systems::advanceEffects(*scene, 0.1f, true);
	scene->shiftWorldOrigin({1000.0, 0.0, 0.0});
	EXPECT_NEAR(emitter.getComponent<component::Trail>().m_points.front().x(), 0.f, 1e-3f);
	systems::advanceEffects(*scene, 0.1f, true);
	const auto& system = emitter.getComponent<component::ParticleEmitter>().m_slot.system;
	for (const auto& particle: system->getParticles()) {
		if (particle.isAlive()) {
			EXPECT_LT(std::abs(particle.position.x()), 5.f);
		}
	}
}

TEST_F(ParticleEmitterTest, ALightKeepsItsFlareStrength) {
	auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Lamp");
	entity.addComponent<component::Light>().flare = 2.5f;
	const auto copy = Scene::copy(scene);
	EXPECT_FLOAT_EQ(copy->findEntityByUUID(entity.getUUID()).getComponent<component::Light>().flare, 2.5f);
}
