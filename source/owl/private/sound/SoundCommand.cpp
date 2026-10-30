/**
 * @file SoundCommand.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "sound/SoundCommand.h"

namespace owl::sound {

uniq<SoundAPI> SoundCommand::m_soundApi = nullptr;
uniq<Mixer> SoundCommand::m_mixer = nullptr;

void SoundCommand::create(const SoundAPI::Type& iType) {
	m_mixer.reset();
	m_soundApi = SoundAPI::create(iType);
	if (m_soundApi)
		m_mixer = mkUniq<Mixer>(*m_soundApi);
}

void SoundCommand::frame(const core::Timestep& iTs) {
	if (!m_soundApi)
		return;
	m_soundApi->frame(iTs);
	m_mixer->update(iTs.getSeconds());
}

auto SoundCommand::getState() -> SoundAPI::State {
	if (m_soundApi)
		return m_soundApi->getState();
	return SoundAPI::State::Error;
}

auto SoundCommand::play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle {
	if (m_mixer)
		return m_mixer->play(iData, iParams);
	return invalidSoundHandle;
}

void SoundCommand::stop(const SoundHandle iHandle) {
	if (m_mixer)
		m_mixer->stop(iHandle);
}

void SoundCommand::pause(const SoundHandle iHandle) {
	if (m_soundApi)
		m_soundApi->pause(iHandle);
}

void SoundCommand::resume(const SoundHandle iHandle) {
	if (m_soundApi)
		m_soundApi->resume(iHandle);
}

void SoundCommand::setVolume(const SoundHandle iHandle, const float iVolume) {
	if (m_mixer)
		m_mixer->setVolume(iHandle, iVolume);
}

void SoundCommand::setFilter(const SoundHandle iHandle, const Filter& iFilter) {
	if (m_mixer)
		m_mixer->setFilter(iHandle, iFilter);
}

void SoundCommand::setListenerVelocity(const math::vec3f& iVelocity) {
	if (m_soundApi)
		m_soundApi->setListenerVelocity(iVelocity);
}

void SoundCommand::setPitch(const SoundHandle iHandle, const float iPitch) {
	if (m_soundApi)
		m_soundApi->setPitch(iHandle, iPitch);
}

void SoundCommand::setLoop(const SoundHandle iHandle, const bool iLoop) {
	if (m_soundApi)
		m_soundApi->setLoop(iHandle, iLoop);
}

void SoundCommand::setPosition(const SoundHandle iHandle, const math::vec3f& iPosition) {
	if (m_soundApi)
		m_soundApi->setPosition(iHandle, iPosition);
}

void SoundCommand::setVelocity(const SoundHandle iHandle, const math::vec3f& iVelocity) {
	if (m_soundApi)
		m_soundApi->setVelocity(iHandle, iVelocity);
}

void SoundCommand::setListenerPosition(const math::vec3f& iPosition) {
	if (m_soundApi)
		m_soundApi->setListenerPosition(iPosition);
}

void SoundCommand::setListenerOrientation(const math::vec3f& iForward, const math::vec3f& iUp) {
	if (m_soundApi)
		m_soundApi->setListenerOrientation(iForward, iUp);
}

void SoundCommand::setListenerGain(const float iGain) {
	if (m_soundApi)
		m_soundApi->setListenerGain(iGain);
}

void SoundCommand::stopAll() {
	if (m_soundApi)
		m_soundApi->stopAll();
	if (m_mixer)
		m_mixer->clear();
}

auto SoundCommand::isPlaying(const SoundHandle iHandle) -> bool {
	if (m_soundApi)
		return m_soundApi->isPlaying(iHandle);
	return false;
}

}// namespace owl::sound
