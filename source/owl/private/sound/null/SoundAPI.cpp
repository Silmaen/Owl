/**
 * @file SoundAPI.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "SoundAPI.h"

#include <tuple>
#include <unordered_map>

namespace owl::sound::null {

void SoundAPI::init() { setState(State::Ready); }

SoundAPI::~SoundAPI() { setState(State::Created); }

void SoundAPI::playSound(const shared<SoundData>& iData) { std::ignore = play(iData, PlayParams{}); }

auto SoundAPI::play(const shared<SoundData>&, const PlayParams& iParams) -> SoundHandle {
	const SoundHandle handle = m_nextHandle++;
	m_sources.insert_or_assign(handle, NullSource{.params = iParams, .paused = false, .environmentSend = false});
	return handle;
}

void SoundAPI::stop(const SoundHandle iHandle) { m_sources.erase(iHandle); }

void SoundAPI::pause(const SoundHandle iHandle) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.paused = true;
}

void SoundAPI::resume(const SoundHandle iHandle) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.paused = false;
}

void SoundAPI::setVolume(const SoundHandle iHandle, const float iVolume) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.params.volume = iVolume;
}

void SoundAPI::setPitch(const SoundHandle iHandle, const float iPitch) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.params.pitch = iPitch;
}

void SoundAPI::setLoop(const SoundHandle iHandle, const bool iLoop) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.params.loop = iLoop;
}

void SoundAPI::setPosition(const SoundHandle iHandle, const math::vec3f& iPosition) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.params.position = iPosition;
}

void SoundAPI::setVelocity(const SoundHandle iHandle, const math::vec3f& iVelocity) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.params.velocity = iVelocity;
}

void SoundAPI::setListenerPosition(const math::vec3f& iPosition) { m_listenerPosition = iPosition; }

void SoundAPI::setListenerOrientation(const math::vec3f&, const math::vec3f&) {}

void SoundAPI::setFilter(const SoundHandle iHandle, const Filter& iFilter) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.params.filter = iFilter;
}

void SoundAPI::setEnvironmentSend(const SoundHandle iHandle, const bool iSend) {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		it->second.environmentSend = iSend;
}

void SoundAPI::setReverb(const Reverb& iReverb, const float iWet) {
	m_reverb = iReverb;
	m_reverbWet = iWet;
}

auto SoundAPI::hasSource(const SoundHandle iHandle) const -> bool { return m_sources.contains(iHandle); }

auto SoundAPI::isPlaying(SoundHandle) const -> bool { return false; }

void SoundAPI::frame(const core::Timestep&) {
	std::erase_if(m_sources, [](const auto& iEntry) -> bool { return !iEntry.second.params.loop; });
}

auto SoundAPI::getSource(const SoundHandle iHandle) const -> const NullSource* {
	const auto it = m_sources.find(iHandle);
	return it != m_sources.end() ? &it->second : nullptr;
}

}// namespace owl::sound::null
