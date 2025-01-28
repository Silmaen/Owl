/**
 * @file SoundData.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/SoundData.h"

#include "null/SoundData.h"
#include "openal/SoundData.h"
#include "sound/SoundSystem.h"

namespace owl::sound {

SoundData::SoundData(const Specification& iSpec) : m_specification{iSpec} {}

SoundData::~SoundData() = default;

auto SoundData::create(const Specification& iSpec) -> shared<SoundData> {
	switch (SoundCommand::getApi()) {
		case SoundAPI::Type::Null:
			return mkShared<null::SoundData>(iSpec);
		case SoundAPI::Type::OpenAL:
			return mkShared<openal::SoundData>(iSpec);
	}
	OWL_CORE_ERROR("Unknown Sound API Type!")
	return nullptr;
}
auto SoundData::create(const std::filesystem::path& iSpec) -> shared<SoundData> {
	return create(Specification{.file = iSpec});
}

}// namespace owl::sound
