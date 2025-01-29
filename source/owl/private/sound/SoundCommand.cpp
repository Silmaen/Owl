/**
 * @file SoundCommand.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "sound/SoundCommand.h"

namespace owl::sound {

uniq<SoundAPI> SoundCommand::m_soundAPI = nullptr;

void SoundCommand::create(const SoundAPI::Type& iType) { m_soundAPI = SoundAPI::create(iType); }

auto SoundCommand::getState() -> SoundAPI::State {
	if (m_soundAPI)
		return m_soundAPI->getState();
	return SoundAPI::State::Error;
}


}// namespace owl::sound
