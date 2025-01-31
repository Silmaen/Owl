
#include "testHelper.h"

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

TEST(Renderer, fakeScene) {
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
