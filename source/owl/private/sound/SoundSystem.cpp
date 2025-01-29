/**
 * @file SoundSystem.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/SoundSystem.h"

namespace owl::sound {

SoundSystem::State SoundSystem::m_internalState = State::Created;
shared<SoundSystem::soundLibrary> SoundSystem::m_soundLibrary = nullptr;

void SoundSystem::init() {
	OWL_PROFILE_FUNCTION()

	m_soundLibrary = std::make_shared<soundLibrary>();

	SoundCommand::init();
	if (SoundCommand::getState() != SoundAPI::State::Ready) {
		m_internalState = State::Error;
		return;
	}

	m_internalState = State::Running;
}

void SoundSystem::shutdown() {
	reset();
	m_internalState = State::Stopped;
}


void SoundSystem::reset() {
	SoundCommand::invalidate();
	m_soundLibrary.reset();
	m_internalState = State::Created;
}

}// namespace owl::sound
