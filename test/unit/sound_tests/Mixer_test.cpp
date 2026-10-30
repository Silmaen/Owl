/**
 * @file Mixer_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <sound/AdaptiveMusic.h>
#include <sound/AudioEffects.h>
#include <sound/Bus.h>
#include <sound/Mixer.h>
#include <sound/SoundCommand.h>
#include <sound/SoundCue.h>
#include <sound/null/SoundAPI.h>

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::sound;

namespace {

class MixerTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_api.init();
	}

	void TearDown() override { core::Log::invalidate(); }

	auto playLoop(const Bus iBus, const float iVolume = 1.f) -> SoundHandle {
		return m_mixer.play(nullptr, PlayParams{.volume = iVolume, .loop = true, .bus = iBus});
	}

	[[nodiscard]] auto backendGain(const SoundHandle iHandle) const -> float {
		const auto* source = m_api.getSource(iHandle);
		return source != nullptr ? source->params.volume : -1.f;
	}

	null::SoundAPI m_api;
	Mixer m_mixer{m_api};
};

}// namespace

TEST(Bus, NamesRoundTripIgnoringCase) {
	for (const Bus bus: g_allBuses) EXPECT_EQ(parseBus(getBusName(bus)), bus);
	EXPECT_EQ(parseBus("music"), Bus::Music);
	EXPECT_EQ(parseBus("sfx"), Bus::SFX);
	EXPECT_FALSE(parseBus("drums").has_value());
	EXPECT_FALSE(isEnvironmentBus(Bus::Music));
	EXPECT_FALSE(isEnvironmentBus(Bus::Master));
	EXPECT_TRUE(isEnvironmentBus(Bus::Voice));
}

TEST_F(MixerTest, BusGainMultipliesVolumesAndMaster) {
	const auto handle = playLoop(Bus::Music, 0.5f);
	EXPECT_FLOAT_EQ(backendGain(handle), 0.5f);
	m_mixer.setBusVolume(Bus::Music, 0.8f);
	m_mixer.setUserVolume(Bus::Music, 0.5f);
	m_mixer.setBusVolume(Bus::Master, 0.5f);
	EXPECT_FLOAT_EQ(m_mixer.getBusGain(Bus::Music), 0.2f);
	EXPECT_FLOAT_EQ(backendGain(handle), 0.1f);
	EXPECT_EQ(m_mixer.getSourceBus(handle), Bus::Music);
	EXPECT_FLOAT_EQ(m_mixer.getSourceGain(handle).value_or(0.f), 0.1f);
	m_mixer.setBusVolume(Bus::SFX, 7.f);
	EXPECT_FLOAT_EQ(m_mixer.getBusVolume(Bus::SFX), 2.f);
}

TEST_F(MixerTest, MuteAndSoloSilenceBuses) {
	const auto music = playLoop(Bus::Music);
	const auto sfx = playLoop(Bus::SFX);
	m_mixer.setBusMuted(Bus::SFX, true);
	EXPECT_FLOAT_EQ(backendGain(sfx), 0.f);
	EXPECT_FLOAT_EQ(backendGain(music), 1.f);
	m_mixer.setBusMuted(Bus::SFX, false);
	m_mixer.setBusSolo(Bus::SFX, true);
	EXPECT_FLOAT_EQ(backendGain(music), 0.f);
	EXPECT_FLOAT_EQ(backendGain(sfx), 1.f);
	m_mixer.setBusSolo(Bus::Master, true);
	EXPECT_FALSE(m_mixer.isBusSolo(Bus::Master));
	m_mixer.setBusSolo(Bus::SFX, false);
	m_mixer.setBusMuted(Bus::Master, true);
	EXPECT_FLOAT_EQ(backendGain(music), 0.f);
	EXPECT_FLOAT_EQ(backendGain(sfx), 0.f);
}

TEST_F(MixerTest, MixSettingsRoundTripAndClamp) {
	MixSettings mix;
	mix.getBus(Bus::Voice).volume = 5.f;
	mix.getBus(Bus::Master).solo = true;
	mix.getBus(Bus::Ambient).muted = true;
	mix.dopplerFactor = 2.f;
	m_mixer.setMix(mix);
	const auto read = m_mixer.getMix();
	EXPECT_FLOAT_EQ(read.getBus(Bus::Voice).volume, 2.f);
	EXPECT_FALSE(read.getBus(Bus::Master).solo);
	EXPECT_TRUE(read.getBus(Bus::Ambient).muted);
	EXPECT_FLOAT_EQ(m_api.getDopplerFactor(), 2.f);
	EXPECT_EQ(read, mix.clamped());
}

TEST(FadeCurve, LinearAndEqualPower) {
	EXPECT_FLOAT_EQ(getFadeGain(0.f, 1.f, 0.25f, FadeCurve::Linear), 0.25f);
	EXPECT_FLOAT_EQ(getFadeGain(0.f, 1.f, 2.f, FadeCurve::Linear), 1.f);
	const float in = getFadeGain(0.f, 1.f, 0.5f, FadeCurve::EqualPower);
	const float out = getFadeGain(1.f, 0.f, 0.5f, FadeCurve::EqualPower);
	EXPECT_NEAR(in, std::numbers::sqrt2_v<float> * 0.5f, 1e-5f);
	// Equal power: the two halves of a crossfade keep a constant power.
	for (const float t: {0.f, 0.1f, 0.3f, 0.7f, 1.f}) {
		const float a = getFadeGain(1.f, 0.f, t, FadeCurve::EqualPower);
		const float b = getFadeGain(0.f, 1.f, t, FadeCurve::EqualPower);
		EXPECT_NEAR(a * a + b * b, 1.f, 1e-5f);
	}
	EXPECT_NEAR(out, in, 1e-6f);
}

TEST_F(MixerTest, FadesAdvanceAndStopAtTheEnd) {
	const auto handle = playLoop(Bus::SFX);
	m_mixer.fadeTo(handle, 0.f, 2.f, FadeCurve::Linear, true);
	EXPECT_TRUE(m_mixer.isFading(handle));
	m_mixer.update(1.f);
	EXPECT_NEAR(backendGain(handle), 0.5f, 1e-5f);
	m_mixer.update(1.f);
	EXPECT_FALSE(m_api.hasSource(handle));
	EXPECT_EQ(m_mixer.getSourceCount(), 0u);
}

TEST_F(MixerTest, CrossfadeMusicTracks) {
	const auto first = playLoop(Bus::Music);
	m_mixer.setMusic(first, 0.f);
	EXPECT_EQ(m_mixer.getMusic(), first);
	const auto second = playLoop(Bus::Music);
	m_mixer.setMusic(second, 2.f);
	EXPECT_EQ(m_mixer.getMusic(), second);
	EXPECT_FLOAT_EQ(backendGain(second), 0.f);
	m_mixer.update(1.f);
	EXPECT_NEAR(backendGain(first), std::cos(std::numbers::pi_v<float> * 0.25f), 1e-5f);
	EXPECT_NEAR(backendGain(second), std::sin(std::numbers::pi_v<float> * 0.25f), 1e-5f);
	m_mixer.update(1.f);
	EXPECT_FALSE(m_api.hasSource(first));
	EXPECT_FLOAT_EQ(backendGain(second), 1.f);
	m_mixer.stopMusic(0.f);
	EXPECT_FALSE(m_api.hasSource(second));
	EXPECT_EQ(m_mixer.getMusic(), invalidSoundHandle);
}

TEST_F(MixerTest, EnvironmentFiltersTheEnvironmentBusesOnly) {
	const auto music = playLoop(Bus::Music);
	const auto sfx = m_mixer.play(nullptr, PlayParams{.loop = true, .bus = Bus::SFX, .filter = {.gainHigh = 0.5f}});
	EXPECT_FALSE(m_api.getSource(music)->environmentSend);
	EXPECT_TRUE(m_api.getSource(sfx)->environmentSend);
	const AudioEnvironment environment{.reverb = getReverbPreset(ReverbPreset::Cave),
									   .wet = 1.f,
									   .filter = {.gainHigh = 0.2f}};
	m_mixer.setEnvironment(environment);
	EXPECT_EQ(m_api.getReverb(), getReverbPreset(ReverbPreset::Cave));
	EXPECT_FLOAT_EQ(m_api.getReverbWet(), 1.f);
	EXPECT_FLOAT_EQ(m_api.getSource(sfx)->params.filter.gainHigh, 0.1f);
	EXPECT_TRUE(m_api.getSource(music)->params.filter.isNeutral());
	m_mixer.setFilter(sfx, {});
	EXPECT_FLOAT_EQ(m_api.getSource(sfx)->params.filter.gainHigh, 0.2f);
}

TEST_F(MixerTest, ForgetsTheSoundsTheBackendReleased) {
	const auto shot = m_mixer.play(nullptr, PlayParams{});
	EXPECT_EQ(m_mixer.getSourceBus(shot), Bus::SFX);
	m_api.frame(core::Timestep{});
	m_mixer.update(0.f);
	EXPECT_FALSE(m_mixer.getSourceBus(shot).has_value());
	EXPECT_FALSE(m_mixer.getVolume(shot).has_value());
}

TEST(AudioEffects, FilterCombineAndNeutral) {
	const Filter lowPass{.gain = 1.f, .gainLow = 1.f, .gainHigh = 0.5f};
	const Filter highPass{.gain = 0.5f, .gainLow = 0.5f, .gainHigh = 1.f};
	const auto both = lowPass.combined(highPass);
	EXPECT_FLOAT_EQ(both.gain, 0.5f);
	EXPECT_FLOAT_EQ(both.gainLow, 0.5f);
	EXPECT_FLOAT_EQ(both.gainHigh, 0.5f);
	EXPECT_TRUE(Filter{}.isNeutral());
	EXPECT_FALSE(lowPass.isNeutral());
	EXPECT_FLOAT_EQ((Filter{.gain = 3.f, .gainLow = -1.f, .gainHigh = 0.5f}.clamped().gain), 1.f);
}

TEST(AudioEffects, PresetsHaveNamesAndValues) {
	EXPECT_EQ(parseReverbPreset("Underwater"), ReverbPreset::Underwater);
	EXPECT_EQ(parseReverbPreset(getReverbPresetName(ReverbPreset::Cave)), ReverbPreset::Cave);
	EXPECT_FALSE(parseReverbPreset("Bathroom").has_value());
	EXPECT_FLOAT_EQ(getReverbPreset(ReverbPreset::Cave).decayTime, 2.91f);
	EXPECT_GT(getReverbPreset(ReverbPreset::Hall).decayTime, getReverbPreset(ReverbPreset::Room).decayTime);
	EXPECT_EQ(getReverbPreset(ReverbPreset::Custom), getReverbPreset(ReverbPreset::Generic));
	const auto mixed = mixReverb(getReverbPreset(ReverbPreset::Room), getReverbPreset(ReverbPreset::Cave), 0.5f);
	EXPECT_FLOAT_EQ(mixed.decayTime, (0.4f + 2.91f) * 0.5f);
}

TEST(AudioEffects, ZoneWeightFadesOverTheBlendDistance) {
	EXPECT_FLOAT_EQ(getZoneWeight(0.f, 2.f), 1.f);
	EXPECT_FLOAT_EQ(getZoneWeight(1.f, 2.f), 0.5f);
	EXPECT_FLOAT_EQ(getZoneWeight(3.f, 2.f), 0.f);
	EXPECT_FLOAT_EQ(getZoneWeight(0.1f, 0.f), 0.f);
}

TEST(AudioEffects, BlendZones) {
	EXPECT_EQ(blendAudioZones({}), AudioEnvironment{});
	const auto cave = getReverbPreset(ReverbPreset::Cave);
	const auto room = getReverbPreset(ReverbPreset::Room);
	const std::array half{AudioZoneSample{.reverb = cave, .filter = {.gainHigh = 0.f}, .weight = 0.5f}};
	const auto edge = blendAudioZones(half);
	EXPECT_FLOAT_EQ(edge.wet, 0.5f);
	EXPECT_EQ(edge.reverb, cave);
	EXPECT_FLOAT_EQ(edge.filter.gainHigh, 0.5f);
	const std::array overlap{AudioZoneSample{.reverb = cave, .filter = {}, .weight = 1.f},
							 AudioZoneSample{.reverb = room, .filter = {}, .weight = 1.f}};
	const auto both = blendAudioZones(overlap);
	EXPECT_FLOAT_EQ(both.wet, 1.f);
	EXPECT_NEAR(both.reverb.decayTime, (cave.decayTime + room.decayTime) * 0.5f, 1e-5f);
}

TEST(AudioEffects, DopplerVelocityFromWorldPositions) {
	const math::vec3d far{1.5e11, 0.0, 0.0};
	const auto velocity = getDopplerVelocity(far, far + math::vec3d{2.0, 0.0, 0.0}, 0.5);
	EXPECT_FLOAT_EQ(velocity.x(), 4.f);
	EXPECT_FLOAT_EQ(velocity.y(), 0.f);
	EXPECT_EQ(getDopplerVelocity(far, far, 0.0), (math::vec3f{0.f, 0.f, 0.f}));
	const auto teleport = getDopplerVelocity({0.0, 0.0, 0.0}, {1.0e6, 0.0, 0.0}, 0.016);
	EXPECT_NEAR(teleport.norm(), g_maxDopplerSpeed, 1e-2f);
}

TEST_F(MixerTest, AdaptiveMusicSwitchesStates) {
	AdaptiveMusic music{.layers = {{.name = "pads", .asset = "pads.wav", .volume = 1.f},
								   {.name = "drums", .asset = "drums.wav", .volume = 0.5f}},
						.states = {{.name = "calm", .layers = {"pads"}, .stinger = {}},
								   {.name = "combat", .layers = {"pads", "drums"}, .stinger = "hit.wav"}},
						.fadeSeconds = 1.f};
	EXPECT_EQ(music.findState("combat"), &music.states[1]);
	EXPECT_EQ(music.findState("nope"), nullptr);
	EXPECT_FLOAT_EQ(music.getLayerGain(music.states[1], 1), 0.5f);
	EXPECT_FLOAT_EQ(music.getLayerGain(music.states[0], 1), 0.f);
	std::vector<std::string> played;
	const AdaptiveMusicPlayer::PlayFunction play = [&](const std::string& iAsset,
													   const PlayParams& iParams) -> SoundHandle {
		played.push_back(iAsset);
		return m_mixer.play(nullptr, iParams);
	};
	AdaptiveMusicPlayer player;
	ASSERT_TRUE(player.start(music, "", m_mixer, play));
	EXPECT_EQ(player.getState(), "calm");
	ASSERT_EQ(player.getLayerHandles().size(), 2u);
	const auto pads = player.getLayerHandles()[0];
	const auto drums = player.getLayerHandles()[1];
	EXPECT_EQ(m_mixer.getSourceBus(pads), Bus::Music);
	EXPECT_FLOAT_EQ(backendGain(pads), 0.f);
	m_mixer.update(1.f);
	EXPECT_FLOAT_EQ(backendGain(pads), 1.f);
	EXPECT_FLOAT_EQ(backendGain(drums), 0.f);
	EXPECT_TRUE(player.setState("combat", m_mixer, play));
	EXPECT_EQ(played.back(), "hit.wav");
	EXPECT_NE(player.getLastStinger(), invalidSoundHandle);
	m_mixer.update(1.f);
	EXPECT_FLOAT_EQ(backendGain(drums), 0.5f);
	EXPECT_FALSE(player.setState("boss", m_mixer, play));
	EXPECT_EQ(player.getState(), "combat");
	player.stop(m_mixer, 0.f);
	EXPECT_FALSE(player.isRunning());
	EXPECT_FALSE(m_api.hasSource(pads));
	EXPECT_FALSE(player.start(AdaptiveMusic{}, "", m_mixer, play));
}

TEST(SoundCommandMixer, RoutesPlayAndVolumeThroughTheMixer) {
	core::Log::init(core::Log::Level::Off);
	SoundCommand::create(SoundAPI::Type::Null);
	SoundCommand::init();
	auto* mixer = SoundCommand::getMixer();
	ASSERT_NE(mixer, nullptr);
	const auto handle = SoundCommand::play(nullptr, PlayParams{.volume = 0.5f, .loop = true, .bus = Bus::Ambient});
	mixer->setBusVolume(Bus::Ambient, 0.5f);
	SoundCommand::setVolume(handle, 1.f);
	EXPECT_FLOAT_EQ(mixer->getSourceGain(handle).value_or(0.f), 0.5f);
	SoundCommand::setFilter(handle, {.gainHigh = 0.3f});
	SoundCommand::setListenerVelocity({1.f, 0.f, 0.f});
	SoundCommand::frame(core::Timestep{});
	EXPECT_EQ(mixer->getSourceCount(), 1u);
	SoundCommand::stopAll();
	EXPECT_EQ(mixer->getSourceCount(), 0u);
	SoundCommand::invalidate();
	EXPECT_EQ(SoundCommand::getMixer(), nullptr);
	core::Log::invalidate();
}

TEST(SoundCue, KeepsItsDefaultBus) {
	SoundCue cue;
	cue.bus = Bus::Voice;
	const auto read = SoundCue::deserializeFromString(cue.serializeToString());
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->bus, Bus::Voice);
	const auto old = SoundCue::deserializeFromString("SoundCue: ''\nvariants: []\n");
	ASSERT_TRUE(old.has_value());
	EXPECT_EQ(old->bus, Bus::SFX);
}
