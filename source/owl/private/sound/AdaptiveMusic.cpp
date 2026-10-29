/**
 * @file AdaptiveMusic.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/AdaptiveMusic.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace owl::sound {

auto MusicState::hasLayer(const std::string_view iLayer) const -> bool {
	return std::ranges::find(layers, iLayer) != layers.end();
}

auto AdaptiveMusic::findState(const std::string_view iName) const -> const MusicState* {
	const auto found = std::ranges::find(states, iName, &MusicState::name);
	return found != states.end() ? &*found : nullptr;
}

auto AdaptiveMusic::getLayerGain(const MusicState& iState, const size_t iLayer) const -> float {
	if (iLayer >= layers.size())
		return 0.f;
	return iState.hasLayer(layers[iLayer].name) ? std::clamp(layers[iLayer].volume, 0.f, 1.f) : 0.f;
}

auto AdaptiveMusicPlayer::start(const AdaptiveMusic& iMusic, const std::string_view iState, Mixer& ioMixer,
								const PlayFunction& iPlay) -> bool {
	stop(ioMixer, 0.f);
	if (iMusic.states.empty() || iMusic.layers.empty()) {
		OWL_CORE_WARN("AdaptiveMusic: No layer or no state to play.")
		return false;
	}
	m_music = iMusic;
	const MusicState* state = m_music.findState(iState);
	if (state == nullptr)
		state = &m_music.states.front();
	m_state = state->name;
	bool started = false;
	for (size_t index = 0; index < m_music.layers.size(); ++index) {
		const auto& layer = m_music.layers[index];
		const SoundHandle handle = iPlay(layer.asset, PlayParams{.volume = 1.f, .loop = true, .bus = Bus::Music});
		m_layers.push_back(handle);
		if (handle == invalidSoundHandle)
			continue;
		started = true;
		ioMixer.setFadeGain(handle, 0.f);
		ioMixer.fadeTo(handle, m_music.getLayerGain(*state, index), m_music.fadeSeconds);
	}
	m_running = started;
	if (!started) {
		OWL_CORE_WARN("AdaptiveMusic: No layer could be played.")
		m_layers.clear();
		m_state.clear();
	}
	return started;
}

auto AdaptiveMusicPlayer::setState(const std::string_view iState, Mixer& ioMixer, const PlayFunction& iPlay) -> bool {
	const MusicState* state = m_music.findState(iState);
	if (state == nullptr) {
		OWL_CORE_WARN("AdaptiveMusic: Unknown state '{}'.", iState)
		return false;
	}
	if (!m_running || m_state == state->name)
		return true;
	m_state = state->name;
	for (size_t index = 0; index < m_layers.size(); ++index)
		ioMixer.fadeTo(m_layers[index], m_music.getLayerGain(*state, index), m_music.fadeSeconds);
	if (!state->stinger.empty())
		m_stinger = iPlay(state->stinger, PlayParams{.volume = 1.f, .loop = false, .bus = Bus::Music});
	return true;
}

void AdaptiveMusicPlayer::stop(Mixer& ioMixer, const float iSeconds) {
	for (const SoundHandle handle: m_layers) {
		if (handle == invalidSoundHandle)
			continue;
		if (iSeconds > 0.f)
			ioMixer.fadeTo(handle, 0.f, iSeconds, FadeCurve::EqualPower, true);
		else
			ioMixer.stop(handle);
	}
	m_layers.clear();
	m_state.clear();
	m_running = false;
}

}// namespace owl::sound
