/**
 * @file SoundAPI.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SoundAPI.h"

namespace owl::sound::null {

void SoundAPI::init() { setState(State::Ready); }

SoundAPI::~SoundAPI() { setState(State::Created); }

void SoundAPI::playSound(const shared<SoundData>&) {}

void SoundAPI::frame(const core::Timestep&) {}

}// namespace owl::sound::null
