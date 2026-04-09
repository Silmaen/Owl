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
#include "sound/SoundSystem.h"

namespace owl::sound {

auto SoundHelper::playEntitySound(const scene::Entity& iEntity) -> SoundHandle {
	if (!iEntity || !iEntity.hasComponent<scene::component::SoundSource>())
		return invalidSoundHandle;

	auto& [soundComp] = iEntity.getComponent<scene::component::SoundSource>();
	if (soundComp.soundAsset.empty())
		return invalidSoundHandle;

	if (SoundSystem::getState() != SoundSystem::State::Running && SoundSystem::getState() != SoundSystem::State::Error)
		return invalidSoundHandle;

	auto& soundLibrary = SoundSystem::getSoundLibrary();
	const auto soundData = soundLibrary.get(soundComp.soundAsset);
	if (!soundData)
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
							.maxDistance = soundComp.maxDistance,
							.rolloff = soundComp.rolloff};
	soundComp.runtimeHandle = SoundCommand::play(soundData, params);
	return soundComp.runtimeHandle;
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
