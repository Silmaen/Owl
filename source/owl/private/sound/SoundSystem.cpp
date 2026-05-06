/**
 * @file SoundSystem.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/SoundSystem.h"

namespace owl::sound {

SoundSystem::State SoundSystem::m_internalState = State::Created;
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
shared<SoundSystem::SoundLibrary> SoundSystem::m_soundLibrary = mkShared<SoundSystem::SoundLibrary>();

void SoundSystem::init() {
	OWL_PROFILE_FUNCTION()

	m_soundLibrary = mkShared<SoundLibrary>();

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
	m_soundLibrary = mkShared<SoundLibrary>();
	m_internalState = State::Created;
}

}// namespace owl::sound
