/**
 * @file SceneAudio_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneAudio.h>
#include <scene/SceneSerializer.h>
#include <scene/SettingsManager.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/null/SoundAPI.h>

#include <numbers>
#include <string>
#include <vector>

using namespace owl;

namespace {

class SceneAudioTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		sound::SoundCommand::create(sound::SoundAPI::Type::Null);
		sound::SoundCommand::init();
		m_scene = mkShared<scene::Scene>();
	}

	void TearDown() override {
		m_scene.reset();
		sound::SoundCommand::invalidate();
		core::Log::invalidate();
	}

	[[nodiscard]] auto reload() const -> shared<scene::Scene> {
		const scene::SceneSerializer saver(m_scene);
		const auto text = saver.serializeToString();
		auto loaded = mkShared<scene::Scene>();
		const scene::SceneSerializer loader(loaded);
		EXPECT_TRUE(loader.deserializeFromBuffer({text.begin(), text.end()}).has_value()) << text;
		return loaded;
	}

	static auto backend() -> const sound::null::SoundAPI& {
		return dynamic_cast<const sound::null::SoundAPI&>(*sound::SoundCommand::getBackend());
	}

	static auto findByName(const shared<scene::Scene>& iScene, const std::string& iName) -> scene::Entity {
		for (const auto& entity: iScene->getAllEntities())
			if (entity.getName() == iName)
				return entity;
		return {};
	}

	[[nodiscard]] auto makeListener(const math::vec3& iPosition) const -> scene::Entity {
		auto listener = m_scene->createEntity("Ear");
		listener.addComponent<scene::component::SoundListener>().primary = true;
		listener.getComponent<scene::component::Transform>().transform.translation() = iPosition;
		return listener;
	}

	shared<scene::Scene> m_scene;
};

}// namespace

TEST(ReverbZone, DistanceFollowsTheEntityTransform) {
	scene::component::ReverbZone zone;
	zone.extents = {1.f, 1.f, 1.f};
	math::Transform transform{{10.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {2.f, 1.f, 1.f}};
	EXPECT_FLOAT_EQ(zone.getDistanceOutside(transform(), {11.5f, 0.f, 0.f}), 0.f);
	EXPECT_NEAR(zone.getDistanceOutside(transform(), {13.f, 0.f, 0.f}), 1.f, 1e-5f);
	EXPECT_NEAR(zone.getDistanceOutside(transform(), {10.f, 3.f, 0.f}), 2.f, 1e-5f);
	transform.rotation() = {0.f, 0.f, std::numbers::pi_v<float> * 0.5f};
	EXPECT_NEAR(zone.getDistanceOutside(transform(), {10.f, 1.5f, 0.f}), 0.f, 1e-4f);
	EXPECT_NEAR(zone.getDistanceOutside(transform(), {11.5f, 0.f, 0.f}), 0.5f, 1e-4f);
	zone.shape = scene::component::ReverbZone::Shape::Sphere;
	zone.radius = 1.f;
	EXPECT_NEAR(zone.getDistanceOutside(transform(), {13.f, 0.f, 0.f}), 1.f, 1e-4f);
	zone.blendDistance = 2.f;
	EXPECT_NEAR(zone.sample(transform(), {13.f, 0.f, 0.f}).weight, 0.5f, 1e-4f);
	zone.preset = sound::ReverbPreset::Custom;
	zone.reverb.decayTime = 9.f;
	EXPECT_FLOAT_EQ(zone.getReverb().decayTime, 9.f);
}

TEST_F(SceneAudioTest, ComponentsAndMixRoundTrip) {
	auto zoneEntity = m_scene->createEntity("Pool");
	auto& zone = zoneEntity.addComponent<scene::component::ReverbZone>();
	zone.shape = scene::component::ReverbZone::Shape::Sphere;
	zone.radius = 4.f;
	zone.preset = sound::ReverbPreset::Custom;
	zone.reverb.decayTime = 5.f;
	zone.filter = {.gain = 1.f, .gainLow = 1.f, .gainHigh = 0.1f};
	auto musicEntity = m_scene->createEntity("Music");
	auto& music = musicEntity.addComponent<scene::component::AdaptiveMusic>();
	music.stateKey = "threat";
	music.initialState = "calm";
	music.music.fadeSeconds = 3.f;
	music.music.layers = {{.name = "pads", .asset = "music/pads.wav", .volume = 0.8f}};
	music.music.states = {{.name = "calm", .layers = {"pads"}, .stinger = "music/hit.wav"}};
	auto sourceEntity = m_scene->createEntity("Radio");
	auto& [source] = sourceEntity.addComponent<scene::component::SoundSource>();
	source.category = sound::Bus::Voice;
	source.filter = {.gain = 1.f, .gainLow = 0.2f, .gainHigh = 1.f};
	source.doppler = false;
	m_scene->getMixSettings().getBus(sound::Bus::Music).volume = 0.4f;
	m_scene->getMixSettings().getBus(sound::Bus::SFX).muted = true;
	m_scene->getMixSettings().dopplerFactor = 0.5f;

	const auto loaded = reload();
	const auto pool = findByName(loaded, "Pool");
	ASSERT_TRUE(pool);
	const auto& readZone = pool.getComponent<scene::component::ReverbZone>();
	EXPECT_EQ(readZone.shape, scene::component::ReverbZone::Shape::Sphere);
	EXPECT_FLOAT_EQ(readZone.radius, 4.f);
	EXPECT_EQ(readZone.preset, sound::ReverbPreset::Custom);
	EXPECT_FLOAT_EQ(readZone.reverb.decayTime, 5.f);
	EXPECT_FLOAT_EQ(readZone.filter.gainHigh, 0.1f);
	const auto& readMusic = findByName(loaded, "Music").getComponent<scene::component::AdaptiveMusic>();
	EXPECT_EQ(readMusic.music, music.music);
	EXPECT_EQ(readMusic.stateKey, "threat");
	EXPECT_EQ(readMusic.initialState, "calm");
	const auto& [readSource] = findByName(loaded, "Radio").getComponent<scene::component::SoundSource>();
	EXPECT_EQ(readSource.category, sound::Bus::Voice);
	EXPECT_FLOAT_EQ(readSource.filter.gainLow, 0.2f);
	EXPECT_FALSE(readSource.doppler);
	EXPECT_EQ(loaded->getMixSettings(), m_scene->getMixSettings());
}

TEST_F(SceneAudioTest, ListenerInsideAZoneHearsItsReverbAndFilter) {
	auto zoneEntity = m_scene->createEntity("Pool");
	auto& zone = zoneEntity.addComponent<scene::component::ReverbZone>();
	zone.preset = sound::ReverbPreset::Underwater;
	zone.filter = {.gain = 1.f, .gainLow = 1.f, .gainHigh = 0.1f};
	zone.blendDistance = 2.f;
	auto listener = makeListener({0.f, 0.f, 0.f});
	scene::updateSceneAudio(*m_scene, 0.016f);
	const auto* mixer = sound::SoundCommand::getMixer();
	EXPECT_FLOAT_EQ(mixer->getEnvironment().wet, 1.f);
	EXPECT_EQ(backend().getReverb(), sound::getReverbPreset(sound::ReverbPreset::Underwater));
	EXPECT_FLOAT_EQ(mixer->getEnvironment().filter.gainHigh, 0.1f);
	listener.getComponent<scene::component::Transform>().transform.translation() = {3.f, 0.f, 0.f};
	scene::updateSceneAudio(*m_scene, 0.016f);
	EXPECT_NEAR(mixer->getEnvironment().wet, 0.5f, 1e-4f);
	listener.getComponent<scene::component::Transform>().transform.translation() = {10.f, 0.f, 0.f};
	scene::updateSceneAudio(*m_scene, 0.016f);
	EXPECT_FLOAT_EQ(mixer->getEnvironment().wet, 0.f);
	EXPECT_TRUE(mixer->getEnvironment().filter.isNeutral());
}

TEST_F(SceneAudioTest, DopplerVelocitiesIgnoreAnOriginShift) {
	auto listener = makeListener({0.f, 0.f, 0.f});
	auto car = m_scene->createEntity("Car");
	auto& [sound] = car.addComponent<scene::component::SoundSource>();
	sound.spatial = true;
	sound.runtimeHandle = sound::SoundCommand::play(nullptr, {.loop = true, .spatial = true});
	scene::updateSceneAudio(*m_scene, 0.5f);
	car.getComponent<scene::component::Transform>().transform.translation() = {5.f, 0.f, 0.f};
	scene::updateSceneAudio(*m_scene, 0.5f);
	EXPECT_FLOAT_EQ(backend().getSource(sound.runtimeHandle)->params.velocity.x(), 10.f);
	m_scene->shiftWorldOrigin({100.0, 0.0, 0.0});
	scene::updateSceneAudio(*m_scene, 0.5f);
	EXPECT_NEAR(backend().getSource(sound.runtimeHandle)->params.velocity.x(), 0.f, 1e-3f);
	EXPECT_NEAR(backend().getListenerVelocity().x(), 0.f, 1e-3f);
	listener.getComponent<scene::component::Transform>().transform.translation() += math::vec3{1.f, 0.f, 0.f};
	scene::updateSceneAudio(*m_scene, 0.5f);
	EXPECT_NEAR(backend().getListenerVelocity().x(), 2.f, 1e-3f);
}

TEST_F(SceneAudioTest, AdaptiveMusicFollowsItsStateKeyAndLua) {
	auto entity = m_scene->createEntity("Music");
	auto& music = entity.addComponent<scene::component::AdaptiveMusic>();
	music.stateKey = "threat";
	music.music.layers = {{.name = "pads", .asset = "missing_pads.wav", .volume = 1.f}};
	music.music.states = {{.name = "calm", .layers = {"pads"}, .stinger = {}},
						  {.name = "danger", .layers = {}, .stinger = {}}};
	m_scene->getGameState().set("threat", std::string{"calm"});
	auto* mixer = sound::SoundCommand::getMixer();
	scene::startAdaptiveMusic(*m_scene, *mixer);
	ASSERT_TRUE(music.runtimePlayer);
	// The layer file is missing: the player does not run, the scene reports no state.
	EXPECT_FALSE(music.runtimePlayer->isRunning());
	EXPECT_EQ(m_scene->getMusicState(), "");
	EXPECT_TRUE(m_scene->setMusicState("danger"));
	EXPECT_FALSE(m_scene->setMusicState("boss"));
}

TEST_F(SceneAudioTest, SettingsGiveThePlayerVolumes) {
	scene::SettingsManager settings;
	settings.setDefault(scene::SettingsManager::KeyVolumeMusic, 0.25f);
	settings.setDefault(scene::SettingsManager::KeyVolumeVoice, 0.5f);
	auto* mixer = sound::SoundCommand::getMixer();
	settings.applyVolumes(*mixer);
	EXPECT_FLOAT_EQ(mixer->getUserVolume(sound::Bus::Music), 0.25f);
	EXPECT_FLOAT_EQ(mixer->getUserVolume(sound::Bus::Voice), 0.5f);
	EXPECT_FLOAT_EQ(mixer->getUserVolume(sound::Bus::SFX), 1.f);
	EXPECT_FLOAT_EQ(mixer->getBusGain(sound::Bus::Music), 0.25f);
}

TEST_F(SceneAudioTest, StartAppliesTheSceneMix) {
	m_scene->getMixSettings().getBus(sound::Bus::Ambient).volume = 0.3f;
	m_scene->onStartRuntime();
	EXPECT_FLOAT_EQ(sound::SoundCommand::getMixer()->getBusVolume(sound::Bus::Ambient), 0.3f);
	m_scene->onEndRuntime();
}
