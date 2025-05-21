/**
 * @file SoundAPI.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "null/SoundAPI.h"
#include "openal/SoundAPI.h"
#include "sound/SoundAPI.h"

namespace owl::sound {

auto SoundAPI::create(const Type& iType) -> uniq<SoundAPI> {
	switch (iType) {
		case Type::Null:
			return mkUniq<null::SoundAPI>();
		case Type::OpenAl:
			return mkUniq<openal::SoundAPI>();
	}

	OWL_CORE_ERROR("Unknown Sound API Type!")
	return nullptr;
}

SoundAPI::~SoundAPI() = default;

}// namespace owl::sound
