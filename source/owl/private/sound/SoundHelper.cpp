/**
 * @file SoundHelper.cpp
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/SoundHelper.h"

#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/SoundSource.h"
#include "scene/component/Transform.h"
#include "sound/SoundCommand.h"
#include "sound/SoundCue.h"
#include "sound/SoundSystem.h"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <string>

namespace owl::sound {

auto SoundHelper::playEntitySound(const scene::Entity& iEntity) -> SoundHandle {
	if (!iEntity || !iEntity.hasComponent<scene::component::SoundSource>())
		return invalidSoundHandle;

	auto& [soundComp] = iEntity.getComponent<scene::component::SoundSource>();
	if (soundComp.soundAsset.empty())
		return invalidSoundHandle;

	if (SoundSystem::getState() != SoundSystem::State::Running && SoundSystem::getState() != SoundSystem::State::Error)
		return invalidSoundHandle;

	math::vec3f position{0, 0, 0};
	if (soundComp.spatial && iEntity.hasComponent<scene::component::Transform>()) {
		if (const auto* scene = iEntity.getScene(); scene != nullptr) {
			const auto wt = scene->getWorldTransform(iEntity);
			position = {wt.translation().x(), wt.translation().y(), wt.translation().z()};
		}
	}

	const PlayParams params{.volume = soundComp.volume,
							.pitch = soundComp.pitch,
							.loop = soundComp.loop,
							.spatial = soundComp.spatial,
							.position = position,
							.velocity = {0.f, 0.f, 0.f},
							.maxDistance = soundComp.maxDistance,
							.rolloff = soundComp.rolloff,
							.bus = soundComp.category,
							.filter = soundComp.filter};
	soundComp.runtimeHandle = playAsset(soundComp.soundAsset, params);
	return soundComp.runtimeHandle;
}

namespace {

std::mutex g_cueMutex;
std::map<std::string, SoundCue, std::less<>> g_cues;

auto getRandom() -> uint64_t {
	static std::mt19937_64 s_engine{std::random_device{}()};
	return s_engine();
}

auto getSoundData(const SoundVariant& iVariant) -> shared<SoundData> {
	auto& library = SoundSystem::getSoundLibrary();
	const auto key = getVariantKey(iVariant);
	if (library.exists(key))
		return library.get(key);
	if (key == iVariant.file)
		return library.load(key);
	auto data = SoundData::create(getVariantSpecification(iVariant));
	if (data)
		library.add(key, data);
	return data;
}

}// namespace

auto SoundHelper::playAsset(const std::string& iAsset, const PlayParams& iParams) -> SoundHandle {
	if (iAsset.empty())
		return invalidSoundHandle;
	if (!SoundCue::isCue(iAsset)) {
		const auto data = getSoundData(
				{.file = iAsset, .trimStart = 0.f, .trimEnd = 0.f, .loopStart = 0.f, .loopEnd = 0.f, .weight = 1.f});
		return data ? SoundCommand::play(data, iParams) : invalidSoundHandle;
	}
	std::optional<SoundCue> cue;
	{
		const std::scoped_lock<std::mutex> lock{g_cueMutex};
		if (const auto found = g_cues.find(iAsset); found != g_cues.end())
			cue = found->second;
	}
	if (!cue) {
		cue = SoundCue::load(iAsset);
		if (!cue)
			return invalidSoundHandle;
		const std::scoped_lock<std::mutex> lock{g_cueMutex};
		g_cues.insert_or_assign(iAsset, *cue);
	}
	const auto pick = cue->pick(getRandom());
	if (!pick)
		return invalidSoundHandle;
	const auto data = getSoundData(cue->variants[pick->variant]);
	if (!data)
		return invalidSoundHandle;
	auto params = iParams;
	params.volume *= pick->volume;
	params.pitch *= pick->pitch;
	params.loop = params.loop || cue->loop;
	if (!params.bus)
		params.bus = cue->bus;
	return SoundCommand::play(data, params);
}

auto SoundHelper::playMusic(const std::string& iAsset, const float iFadeSeconds) -> SoundHandle {
	auto* mixer = SoundCommand::getMixer();
	if (mixer == nullptr) {
		OWL_CORE_WARN("SoundHelper: No sound backend to play the music '{}'.", iAsset)
		return invalidSoundHandle;
	}
	const SoundHandle track = playAsset(iAsset, PlayParams{.volume = 1.f, .loop = true, .bus = Bus::Music});
	if (track == invalidSoundHandle) {
		OWL_CORE_WARN("SoundHelper: Music '{}' could not be played.", iAsset)
		return invalidSoundHandle;
	}
	mixer->setMusic(track, iFadeSeconds);
	return track;
}

void SoundHelper::forgetCue(const std::string& iAsset) {
	const std::scoped_lock<std::mutex> lock{g_cueMutex};
	g_cues.erase(iAsset);
}

void SoundHelper::stopEntitySound(const scene::Entity& iEntity) {
	if (!iEntity || !iEntity.hasComponent<scene::component::SoundSource>())
		return;
	if (auto& [soundComp] = iEntity.getComponent<scene::component::SoundSource>();
		soundComp.runtimeHandle != invalidSoundHandle) {
		SoundCommand::stop(soundComp.runtimeHandle);
		soundComp.runtimeHandle = invalidSoundHandle;
	}
}

}// namespace owl::sound
