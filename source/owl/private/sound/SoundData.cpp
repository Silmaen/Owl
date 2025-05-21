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
SoundData::SoundData(Specification&& iSpec) : m_specification{std::move(iSpec)} {}

SoundData::~SoundData() = default;

auto SoundData::create(const Specification& iSpec) -> shared<SoundData> {
	switch (SoundCommand::getApi()) {
		case SoundAPI::Type::Null:
			return mkShared<null::SoundData>(iSpec);
		case SoundAPI::Type::OpenAl:
			return mkShared<openal::SoundData>(iSpec);
	}
	OWL_CORE_ERROR("Unknown Sound API Type!")
	return nullptr;
}
auto SoundData::create(const std::filesystem::path& iPath) -> shared<SoundData> {
	return create(Specification{.file = iPath});
}

}// namespace owl::sound
