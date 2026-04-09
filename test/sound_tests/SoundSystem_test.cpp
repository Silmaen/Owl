
#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/SoundSource.h>
#include <scene/component/Transform.h>
#include <sound/SoundHelper.h>
#include <sound/SoundSystem.h>

using namespace owl::sound;

TEST(SoundSystem, creation) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundSystem::reset();
	EXPECT_EQ(SoundSystem::getState(), SoundSystem::State::Created);
	SoundSystem::init();
	EXPECT_EQ(SoundSystem::getState(), SoundSystem::State::Error);
	SoundCommand::create(SoundAPI::Type{255});
	EXPECT_EQ(SoundSystem::getState(), SoundSystem::State::Error);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();
	EXPECT_EQ(SoundSystem::getState(), SoundSystem::State::Running);
	SoundSystem::shutdown();
	EXPECT_EQ(SoundSystem::getState(), SoundSystem::State::Stopped);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundSystem, fakeScene) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();
	const owl::shared<SoundData> data = SoundData::create("");
	EXPECT_EQ(data->getSystemId(), 0);
	SoundCommand::playSound(data);
	SoundCommand::frame(owl::core::Timestep{});
	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundSystem, playWithParams) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();
	const owl::shared<SoundData> data = SoundData::create("");

	const PlayParams params{.volume = 0.5f, .pitch = 1.2f, .loop = true, .spatial = true};
	const SoundHandle handle = SoundCommand::play(data, params);
	EXPECT_NE(handle, invalidSoundHandle);

	// Null backend: isPlaying always returns false
	EXPECT_FALSE(SoundCommand::isPlaying(handle));

	// Verify these don't crash
	SoundCommand::setVolume(handle, 0.8f);
	SoundCommand::setPitch(handle, 0.9f);
	SoundCommand::setLoop(handle, false);
	SoundCommand::setPosition(handle, {1.0f, 2.0f, 3.0f});
	SoundCommand::setVelocity(handle, {0.1f, 0.2f, 0.3f});
	SoundCommand::pause(handle);
	SoundCommand::resume(handle);
	SoundCommand::stop(handle);

	SoundCommand::frame(owl::core::Timestep{});
	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundSystem, listenerControl) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	// Verify these don't crash
	SoundCommand::setListenerPosition({5.0f, 10.0f, 0.0f});
	SoundCommand::setListenerOrientation({0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f});

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundSystem, multipleHandles) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();
	const owl::shared<SoundData> data = SoundData::create("");

	const SoundHandle h1 = SoundCommand::play(data, PlayParams{});
	const SoundHandle h2 = SoundCommand::play(data, PlayParams{.loop = true});
	const SoundHandle h3 = SoundCommand::play(data, PlayParams{.spatial = true, .position = {1, 2, 3}});

	EXPECT_NE(h1, invalidSoundHandle);
	EXPECT_NE(h2, invalidSoundHandle);
	EXPECT_NE(h3, invalidSoundHandle);
	EXPECT_NE(h1, h2);
	EXPECT_NE(h2, h3);

	SoundCommand::stop(h1);
	SoundCommand::stop(h2);
	SoundCommand::stop(h3);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundSystem, invalidHandle) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	// Operations on invalid handle should not crash
	SoundCommand::stop(invalidSoundHandle);
	SoundCommand::pause(invalidSoundHandle);
	SoundCommand::resume(invalidSoundHandle);
	SoundCommand::setVolume(invalidSoundHandle, 1.0f);
	SoundCommand::setPitch(invalidSoundHandle, 1.0f);
	SoundCommand::setLoop(invalidSoundHandle, true);
	SoundCommand::setPosition(invalidSoundHandle, {0, 0, 0});
	SoundCommand::setVelocity(invalidSoundHandle, {0, 0, 0});
	EXPECT_FALSE(SoundCommand::isPlaying(invalidSoundHandle));

	// Operations without API should not crash
	SoundCommand::invalidate();
	SoundCommand::stop(42);
	EXPECT_FALSE(SoundCommand::isPlaying(42));
	EXPECT_EQ(SoundCommand::play(nullptr, PlayParams{}), invalidSoundHandle);

	owl::core::Log::invalidate();
}

TEST(SoundSystem, extensions) {
	const auto exts = SoundData::extension();
	EXPECT_EQ(exts.size(), 4);
	EXPECT_EQ(exts[0], ".wav");
	EXPECT_EQ(exts[1], ".ogg");
	EXPECT_EQ(exts[2], ".flac");
	EXPECT_EQ(exts[3], ".mp3");
}

TEST(SoundHelper, playAndStopEntitySound) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("SoundEntity");
	auto& soundSrc = entity.addComponent<owl::scene::component::SoundSource>();
	soundSrc.sound.soundAsset = "test.wav";
	soundSrc.sound.volume = 0.7f;
	soundSrc.sound.loop = true;

	// playEntitySound with Null backend — handle is valid but sound data won't load (empty asset library)
	const SoundHandle handle = SoundHelper::playEntitySound(entity);
	// With no actual sound file in the library, the handle will be invalid
	EXPECT_EQ(handle, invalidSoundHandle);

	// stopEntitySound should not crash even with invalid handle
	SoundHelper::stopEntitySound(entity);
	EXPECT_EQ(soundSrc.sound.runtimeHandle, invalidSoundHandle);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundHelper, entityWithoutComponent) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("NoSoundEntity");

	// Should return invalidSoundHandle without crashing
	EXPECT_EQ(SoundHelper::playEntitySound(entity), invalidSoundHandle);
	SoundHelper::stopEntitySound(entity);// should not crash

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(SoundHelper, emptyAsset) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundSystem::init();

	auto scene = owl::mkShared<owl::scene::Scene>();
	auto entity = scene->createEntity("EmptyAsset");
	auto& soundSrc = entity.addComponent<owl::scene::component::SoundSource>();
	soundSrc.sound.soundAsset = "";

	EXPECT_EQ(SoundHelper::playEntitySound(entity), invalidSoundHandle);

	SoundCommand::invalidate();
	owl::core::Log::invalidate();
}
