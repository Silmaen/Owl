/**
 * @file SceneAudio.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "SceneAudio.h"

#include "app/EngineContext.h"
#include "scene/Entity.h"
#include "scene/component/AdaptiveMusic.h"
#include "scene/component/ReverbZone.h"
#include "scene/component/SoundListener.h"
#include "scene/component/SoundSource.h"
#include "scene/component/Transform.h"
#include "sound/SoundCommand.h"
#include "sound/SoundHelper.h"

#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace owl::scene {

namespace {

auto playAsset(const std::string& iAsset, const sound::PlayParams& iParams) -> sound::SoundHandle {
	return sound::SoundHelper::playAsset(iAsset, iParams);
}

auto toVec3d(const math::vec3& iVector) -> math::vec3d {
	return {static_cast<double>(iVector.x()), static_cast<double>(iVector.y()), static_cast<double>(iVector.z())};
}

struct ListenerFrame {
	math::vec3 position{0.f, 0.f, 0.f};
	float rotationZ{0.f};
};

auto findListener(Scene& ioScene) -> std::optional<std::pair<Entity, ListenerFrame>> {
	for (const auto view = ioScene.registry.view<component::Transform, component::SoundListener>();
		 const auto entity: view) {
		if (!view.get<component::SoundListener>(entity).primary)
			continue;
		const Entity listener{entity, &ioScene};
		const auto world = ioScene.getWorldTransform(listener);
		return std::pair{listener, ListenerFrame{.position = world.translation(), .rotationZ = world.rotation().z()}};
	}
	return std::nullopt;
}

void updateEnvironment(Scene& ioScene, const math::vec3& iListener, sound::Mixer& ioMixer) {
	std::vector<sound::AudioZoneSample> zones;
	for (const auto view = ioScene.registry.view<component::Transform, component::ReverbZone>();
		 const auto entity: view) {
		const Entity zone{entity, &ioScene};
		if (!ioScene.isEffectivelyVisible(zone, false))
			continue;
		const auto sample =
				view.get<component::ReverbZone>(entity).sample(ioScene.getWorldTransform(zone)(), iListener);
		if (sample.weight > 0.f)
			zones.push_back(sample);
	}
	ioMixer.setEnvironment(sound::blendAudioZones(zones));
}

void followStateKeys(Scene& ioScene, sound::Mixer& ioMixer) {
	for (auto&& [entity, music]: ioScene.registry.view<component::AdaptiveMusic>().each()) {
		if (music.stateKey.empty() || !music.runtimePlayer || !music.runtimePlayer->isRunning())
			continue;
		const auto value = ioScene.getGameState().get(music.stateKey);
		if (!value)
			continue;
		if (const auto* state = std::get_if<std::string>(&*value);
			state != nullptr && *state != music.runtimePlayer->getState())
			std::ignore = music.runtimePlayer->setState(*state, ioMixer, &playAsset);
	}
}

}// namespace

void startAdaptiveMusic(Scene& ioScene, sound::Mixer& ioMixer) {
	for (auto&& [entity, music]: ioScene.registry.view<component::AdaptiveMusic>().each()) {
		if (!music.playOnStart)
			continue;
		std::string state = music.initialState;
		if (!music.stateKey.empty()) {
			if (const auto value = ioScene.getGameState().get(music.stateKey); value)
				if (const auto* text = std::get_if<std::string>(&*value); text != nullptr)
					state = *text;
		}
		music.runtimePlayer = mkShared<sound::AdaptiveMusicPlayer>();
		std::ignore = music.runtimePlayer->start(music.music, state, ioMixer, &playAsset);
	}
}

void updateSceneAudio(Scene& ioScene, const float iSeconds) {
	OWL_PROFILE_FUNCTION()

	const auto& origin = ioScene.getWorldOrigin();
	if (const auto listener = findListener(ioScene); listener) {
		const auto& [entity, frame] = *listener;
		sound::SoundCommand::setListenerPosition(frame.position);
		sound::SoundCommand::setListenerOrientation({std::sin(frame.rotationZ), std::cos(frame.rotationZ), 0.0f},
													{0.0f, 0.0f, 1.0f});
		auto& listenerComp = entity.getComponent<component::SoundListener>();
		const math::vec3d world = origin + toVec3d(frame.position);
		sound::SoundCommand::setListenerVelocity(
				listenerComp.runtimeHasLastPosition ? sound::getDopplerVelocity(listenerComp.runtimeLastPosition, world,
																				static_cast<double>(iSeconds))
													: math::vec3f{0.f, 0.f, 0.f});
		listenerComp.runtimeLastPosition = world;
		listenerComp.runtimeHasLastPosition = true;
		if (auto* mixer = sound::SoundCommand::getMixer(); mixer != nullptr)
			updateEnvironment(ioScene, frame.position, *mixer);
	}
	for (const auto view = ioScene.registry.view<component::Transform, component::SoundSource>();
		 const auto entity: view) {
		auto& [soundComp] = view.get<component::SoundSource>(entity);
		if (soundComp.runtimeHandle == sound::invalidSoundHandle || !soundComp.spatial)
			continue;
		const math::vec3 local = ioScene.getWorldTransform(Entity{entity, &ioScene}).translation();
		sound::SoundCommand::setPosition(soundComp.runtimeHandle, local);
		const math::vec3d world = origin + toVec3d(local);
		if (soundComp.doppler && soundComp.runtimeHasLastPosition)
			sound::SoundCommand::setVelocity(
					soundComp.runtimeHandle,
					sound::getDopplerVelocity(soundComp.runtimeLastPosition, world, static_cast<double>(iSeconds)));
		soundComp.runtimeLastPosition = world;
		soundComp.runtimeHasLastPosition = true;
	}
	if (auto* mixer = sound::SoundCommand::getMixer(); mixer != nullptr)
		followStateKeys(ioScene, *mixer);
}

void updateSubtitles(Scene& ioScene, const float iSeconds) {
	OWL_PROFILE_FUNCTION()

	auto& subtitles = ioScene.getSubtitles();
	const auto listener = findListener(ioScene);
	const auto* context = ioScene.getEngineContext();
	for (const auto view = ioScene.registry.view<component::Transform, component::SoundSource>();
		 const auto entity: view) {
		const auto& [soundComp] = view.get<component::SoundSource>(entity);
		if (soundComp.subtitle.empty() || soundComp.runtimeHandle == sound::invalidSoundHandle ||
			!sound::SoundCommand::isPlaying(soundComp.runtimeHandle))
			continue;
		if (soundComp.spatial && listener) {
			const math::vec3 offset =
					ioScene.getWorldTransform(Entity{entity, &ioScene}).translation() - listener->second.position;
			if (offset.normSq() > soundComp.maxDistance * soundComp.maxDistance)
				continue;
		}
		subtitles.show(context != nullptr ? context->translate(soundComp.subtitle)
										  : std::string_view{soundComp.subtitle},
					   Subtitles::g_lingerSeconds);
	}
	subtitles.update(iSeconds);
}

auto Scene::setMusicState(const std::string_view iState) -> bool {
	auto* mixer = sound::SoundCommand::getMixer();
	if (mixer == nullptr) {
		OWL_CORE_WARN("Scene: No sound backend to set the music state '{}'.", iState)
		return false;
	}
	bool known = false;
	for (auto&& [entity, music]: registry.view<component::AdaptiveMusic>().each()) {
		if (!music.runtimePlayer)
			continue;
		known = music.runtimePlayer->setState(iState, *mixer, &playAsset) || known;
	}
	if (!known)
		OWL_CORE_WARN("Scene: No running adaptive music has the state '{}'.", iState)
	return known;
}

auto Scene::getMusicState() const -> std::string {
	for (auto&& [entity, music]: registry.view<component::AdaptiveMusic>().each())
		if (music.runtimePlayer && music.runtimePlayer->isRunning())
			return music.runtimePlayer->getState();
	return {};
}

}// namespace owl::scene
