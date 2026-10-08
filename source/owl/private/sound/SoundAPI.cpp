/**
 * @file SoundAPI.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "null/SoundAPI.h"
#include "sound/SoundAPI.h"
#if OWL_WITH_AUDIO
#include "openal/SoundAPI.h"
#endif

namespace owl::sound {

auto SoundAPI::create(const Type& iType) -> uniq<SoundAPI> {
	switch (iType) {
		case Type::Null:
			return mkUniq<null::SoundAPI>();
		case Type::OpenAl:
#if OWL_WITH_AUDIO
			return mkUniq<openal::SoundAPI>();
#else
			// OpenAL not built (OWL_MODULE_AUDIO=OFF): the engine stays silent.
			OWL_CORE_WARN("SoundAPI: OpenAL not built in (OWL_MODULE_AUDIO=OFF), using the Null backend.")
			return mkUniq<null::SoundAPI>();
#endif
	}

	OWL_CORE_ERROR("Unknown Sound API Type!")
	return nullptr;
}

SoundAPI::~SoundAPI() = default;

}// namespace owl::sound
