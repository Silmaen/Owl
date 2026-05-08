/**
 * @file SoundAPI.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "SoundAPI.h"

namespace owl::sound::null {

void SoundAPI::init() { setState(State::Ready); }

SoundAPI::~SoundAPI() { setState(State::Created); }

void SoundAPI::playSound(const shared<SoundData>&) {}

auto SoundAPI::play(const shared<SoundData>&, const PlayParams&) -> SoundHandle { return m_nextHandle++; }

void SoundAPI::stop(SoundHandle) {}

void SoundAPI::pause(SoundHandle) {}

void SoundAPI::resume(SoundHandle) {}

void SoundAPI::setVolume(SoundHandle, float) {}

void SoundAPI::setPitch(SoundHandle, float) {}

void SoundAPI::setLoop(SoundHandle, bool) {}

void SoundAPI::setPosition(SoundHandle, const math::vec3f&) {}

void SoundAPI::setVelocity(SoundHandle, const math::vec3f&) {}

void SoundAPI::setListenerPosition(const math::vec3f&) {}

void SoundAPI::setListenerOrientation(const math::vec3f&, const math::vec3f&) {}

auto SoundAPI::isPlaying(SoundHandle) const -> bool { return false; }

void SoundAPI::frame(const core::Timestep&) {}

}// namespace owl::sound::null
