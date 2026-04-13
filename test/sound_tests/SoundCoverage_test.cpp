/**
 * @file SoundCoverage_test.cpp
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/SoundSource.h>
#include <scene/component/Transform.h>
#include <sound/SoundHelper.h>
#include <sound/SoundSystem.h>

using namespace owl::sound;

TEST(SoundCoverage, playEntitySoundWhenNotRunning) {
	owl::core::Log::init(owl::core::Log::Level::Off);

	// Without initializing SoundSystem, state should not be Running.
	SoundSystem::reset();
	EXPECT_NE(SoundSystem::getState(), SoundSystem::State::Running);

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("SndEntity");
	auto& soundSrc = entity.addComponent<owl::scene::component::SoundSource>();
	soundSrc.sound.soundAsset = "test.wav";

	// Should return invalidSoundHandle because SoundSystem is not Running.
	EXPECT_EQ(SoundHelper::playEntitySound(entity), invalidSoundHandle);

	owl::core::Log::invalidate();
}

TEST(SoundCoverage, playEntitySoundWithSpatialAndTransform) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("SpatialSnd");
	auto& soundSrc = entity.addComponent<owl::scene::component::SoundSource>();
	soundSrc.sound.soundAsset = "spatial.wav";
	soundSrc.sound.spatial = true;
	soundSrc.sound.maxDistance = 100.0f;
	soundSrc.sound.rolloff = 2.0f;

	// Set a transform position.
	auto& transform = entity.getComponent<owl::scene::component::Transform>();
	transform.transform.translation() = {10.0f, 20.0f, 0.0f};

	// With Null backend, the sound data won't actually be in the library,
	// so playEntitySound will fail at the soundData lookup.
	// This still exercises the early state and component checks.
	const SoundHandle handle = SoundHelper::playEntitySound(entity);
	EXPECT_EQ(handle, invalidSoundHandle);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundCoverage, playEntitySoundWithSpatialNoTransform) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("NoTransformSnd");
	auto& soundSrc = entity.addComponent<owl::scene::component::SoundSource>();
	soundSrc.sound.soundAsset = "spatial.wav";
	soundSrc.sound.spatial = true;

	// Every entity already has a Transform, so test without spatial flag and with it.
	// (Transform is mandatory, so we just test the spatial path.)
	const SoundHandle handle = SoundHelper::playEntitySound(entity);
	EXPECT_EQ(handle, invalidSoundHandle);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundCoverage, stopEntitySoundWithActiveHandle) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("StopTest");
	auto& soundSrc = entity.addComponent<owl::scene::component::SoundSource>();
	soundSrc.sound.soundAsset = "test.wav";

	// Simulate an active handle by setting it manually.
	soundSrc.sound.runtimeHandle = 42;

	// stopEntitySound should stop the handle and reset it.
	SoundHelper::stopEntitySound(entity);
	EXPECT_EQ(soundSrc.sound.runtimeHandle, invalidSoundHandle);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundCoverage, stopEntitySoundInvalidEntity) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	// Default-constructed entity is invalid.
	owl::scene::Entity invalidEntity;
	SoundHelper::stopEntitySound(invalidEntity);// Should not crash.

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundCoverage, playEntitySoundInvalidEntity) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	owl::scene::Entity invalidEntity;
	EXPECT_EQ(SoundHelper::playEntitySound(invalidEntity), invalidSoundHandle);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundCoverage, playEntitySoundInStoppedState) {
	owl::core::Log::init(owl::core::Log::Level::Off);

	// Start, then stop the system to reach Stopped state.
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();
	EXPECT_EQ(SoundSystem::getState(), SoundSystem::State::Running);
	SoundSystem::shutdown();
	EXPECT_EQ(SoundSystem::getState(), SoundSystem::State::Stopped);

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("StoppedState");
	auto& soundSrc = entity.addComponent<owl::scene::component::SoundSource>();
	soundSrc.sound.soundAsset = "test.wav";

	// Stopped state (neither Running nor Error) triggers early-return.
	EXPECT_EQ(SoundHelper::playEntitySound(entity), invalidSoundHandle);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundCoverage, playEntitySoundInCreatedState) {
	owl::core::Log::init(owl::core::Log::Level::Off);

	// Reset to Created state.
	SoundSystem::reset();
	EXPECT_EQ(SoundSystem::getState(), SoundSystem::State::Created);

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("CreatedState");
	auto& soundSrc = entity.addComponent<owl::scene::component::SoundSource>();
	soundSrc.sound.soundAsset = "test.wav";

	// Created state (neither Running nor Error) triggers early-return.
	EXPECT_EQ(SoundHelper::playEntitySound(entity), invalidSoundHandle);

	owl::core::Log::invalidate();
}
