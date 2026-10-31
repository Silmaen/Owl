/**
 * @file SequencePlayback.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SequencePlayback.h"

#include "app/EngineContext.h"
#include "math/Transform.h"
#include "math/linAlgebra.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/Camera.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/LuaScript.h"
#include "scene/component/SequencePlayer.h"
#include "scene/component/Transform.h"
#include "script/ScriptInstance.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace owl::scene {

namespace {

using component::SequencePlayer;

auto loadSequence(const Scene& iScene, const std::string& iAsset) -> shared<const data::sequence::Sequence> {
	if (iAsset.empty()) {
		OWL_CORE_WARN("Sequence: The player names no sequence.")
		return nullptr;
	}
	if (auto* context = iScene.getEngineContext(); context != nullptr)
		return context->getSequences().get(iAsset);
	auto loaded = data::sequence::Sequence::load(iAsset);
	return loaded ? mkShared<const data::sequence::Sequence>(std::move(*loaded)) : nullptr;
}

auto cameraOf(const Entity& iPlayer, const SequencePlayer& iComponent) -> Entity {
	const Entity camera =
			iComponent.camera == core::UUID{0} ? iPlayer : iPlayer.getScene()->findEntityByUUID(iComponent.camera);
	return camera && camera.hasComponent<component::Camera>() ? camera : Entity{};
}

auto parentWorld(const Scene& iScene, const Entity& iEntity) -> math::mat4 {
	if (const auto parentId = iEntity.getComponent<component::Hierarchy>().parentId; parentId != core::UUID{0}) {
		if (const auto parent = iScene.findEntityByUUID(parentId); parent)
			return iScene.getWorldTransform(parent)();
	}
	return math::identity<float, 4>();
}

// The function in the script of the player, else in the scene script.
void callFunction(const Entity& iPlayer, const std::string& iName) {
	if (iName.empty())
		return;
	if (iPlayer.hasComponent<component::LuaScript>()) {
		if (const auto& script = iPlayer.getComponent<component::LuaScript>();
			script.instance && script.instance->isValid() && script.instance->hasFunction(iName)) {
			std::ignore = script.instance->callFunction(iName);
			return;
		}
	}
	if (const auto& script = std::as_const(*iPlayer.getScene()).getSceneScript();
		script.instance && script.instance->isValid() && script.instance->hasFunction(iName))
		std::ignore = script.instance->callFunction(iName);
}

void takeCamera(const Entity& iPlayer, SequencePlayer& ioComponent) {
	const Entity target = cameraOf(iPlayer, ioComponent);
	if (!target)
		return;
	ioComponent.m_replacedCameras.clear();
	auto& registry = iPlayer.getScene()->registry;
	for (const auto view = registry.view<component::Camera, component::ID>(); const auto handle: view) {
		auto& camera = view.get<component::Camera>(handle);
		if (handle == static_cast<entt::entity>(target) || !camera.primary)
			continue;
		camera.primary = false;
		ioComponent.m_replacedCameras.push_back(view.get<component::ID>(handle).id);
	}
	target.getComponent<component::Camera>().primary = true;
}

void releaseCamera(const Entity& iPlayer, SequencePlayer& ioComponent) {
	if (ioComponent.m_replacedCameras.empty())
		return;
	auto* scene = iPlayer.getScene();
	if (const Entity target = cameraOf(iPlayer, ioComponent); target)
		target.getComponent<component::Camera>().primary = false;
	for (const auto uuid: ioComponent.m_replacedCameras)
		if (const Entity camera = scene->findEntityByUUID(uuid); camera && camera.hasComponent<component::Camera>())
			camera.getComponent<component::Camera>().primary = true;
	ioComponent.m_replacedCameras.clear();
}

void applyPose(const Entity& iPlayer, const SequencePlayer& iComponent) {
	const Entity target = cameraOf(iPlayer, iComponent);
	if (!target || !iComponent.m_sequence)
		return;
	const auto pose = getSequenceCameraPose(iPlayer, *iComponent.m_sequence, iComponent.m_time);
	if (!pose)
		return;
	const math::Transform local{math::inverse(parentWorld(*iPlayer.getScene(), target)) * pose->world};
	auto& transform = target.getComponent<component::Transform>().transform;
	transform.translation() = local.translation();
	transform.rotation() = local.rotation();
	auto& camera = target.getComponent<component::Camera>().camera;
	if (camera.getProjectionType() == SceneCamera::ProjectionType::Perspective)
		camera.setPerspectiveVerticalFov(pose->fov);
	else
		camera.setOrthographicSize(pose->size);
}

// Call the events of a span; false when one of them stopped, restarted or moved the playback.
auto callEvents(const Entity& iPlayer, const data::sequence::Sequence& iSequence, const float iFrom, const float iTo,
				const bool iIncludeFrom) -> bool {
	const uint32_t generation = iPlayer.getComponent<SequencePlayer>().m_generation;
	return std::ranges::all_of(iSequence.getEvents(iFrom, iTo, iIncludeFrom),
							   [&iPlayer, generation](const data::sequence::EventKey* iEvent) -> bool {
								   callFunction(iPlayer, iEvent->function);
								   if (!iPlayer || !iPlayer.hasComponent<SequencePlayer>())
									   return false;
								   const auto& player = iPlayer.getComponent<SequencePlayer>();
								   return player.m_playing && player.m_generation == generation;
							   });
}

void advance(const Entity& iPlayer, const float iSeconds) {
	auto& player = iPlayer.getComponent<SequencePlayer>();
	const auto sequence = player.m_sequence;
	const float duration = sequence->duration;
	const float from = player.m_time;
	const bool includeFrom = !player.m_eventsDone;
	player.m_eventsDone = true;
	const float to = from + std::max(0.f, player.speed) * iSeconds;
	const bool loops = player.loop && duration > 0.f;
	if (to < duration) {
		if (!callEvents(iPlayer, *sequence, from, to, includeFrom))
			return;
		iPlayer.getComponent<SequencePlayer>().m_time = to;
		applyPose(iPlayer, iPlayer.getComponent<SequencePlayer>());
		return;
	}
	if (!callEvents(iPlayer, *sequence, from, duration, includeFrom))
		return;
	if (loops) {
		const float wrapped = std::fmod(to, duration);
		if (!callEvents(iPlayer, *sequence, 0.f, wrapped, true))
			return;
		auto& looped = iPlayer.getComponent<SequencePlayer>();
		looped.m_time = wrapped;
		applyPose(iPlayer, looped);
		return;
	}
	auto& ended = iPlayer.getComponent<SequencePlayer>();
	ended.m_time = duration;
	applyPose(iPlayer, ended);
	std::ignore = stopSequence(iPlayer);
}

}// namespace

auto playSequence(const Entity& iEntity, const float iFrom) -> bool {
	if (!iEntity || !iEntity.hasComponent<SequencePlayer>()) {
		OWL_CORE_WARN("Sequence: The entity has no sequence player.")
		return false;
	}
	auto& player = iEntity.getComponent<SequencePlayer>();
	auto sequence = loadSequence(*iEntity.getScene(), player.sequence);
	if (!sequence) {
		OWL_CORE_WARN("Sequence: '{}' cannot be played.", player.sequence)
		return false;
	}
	player.m_sequence = std::move(sequence);
	player.m_time = std::clamp(iFrom, 0.f, player.m_sequence->duration);
	player.m_eventsDone = false;
	++player.m_generation;
	if (!player.m_playing)
		takeCamera(iEntity, player);
	player.m_playing = true;
	player.m_started = true;
	applyPose(iEntity, player);
	return true;
}

auto stopSequence(const Entity& iEntity) -> bool {
	if (!iEntity || !iEntity.hasComponent<SequencePlayer>() || !iEntity.getComponent<SequencePlayer>().m_playing)
		return false;
	auto& player = iEntity.getComponent<SequencePlayer>();
	player.m_playing = false;
	++player.m_generation;
	releaseCamera(iEntity, player);
	callFunction(iEntity, std::string{player.onFinished});
	return true;
}

auto seekSequence(const Entity& iEntity, const float iTime) -> bool {
	if (!iEntity || !iEntity.hasComponent<SequencePlayer>())
		return false;
	auto& player = iEntity.getComponent<SequencePlayer>();
	if (!player.m_playing || !player.m_sequence)
		return false;
	player.m_time = std::clamp(iTime, 0.f, player.m_sequence->duration);
	player.m_eventsDone = true;
	++player.m_generation;
	applyPose(iEntity, player);
	return true;
}

auto getSequenceCameraPose(const Entity& iEntity, const data::sequence::Sequence& iSequence, const float iTime)
		-> std::optional<SequenceCameraPose> {
	if (!iEntity)
		return std::nullopt;
	const auto shot = iSequence.sampleCamera(iTime);
	if (!shot)
		return std::nullopt;
	const math::Transform key{shot->pose.position, shot->pose.rotation};
	const bool drivesItself =
			iEntity.hasComponent<SequencePlayer>() && iEntity.getComponent<SequencePlayer>().camera == core::UUID{0};
	const math::mat4 anchor =
			drivesItself ? parentWorld(*iEntity.getScene(), iEntity) : iEntity.getScene()->getWorldTransform(iEntity)();
	return SequenceCameraPose{.world = anchor * key(),
							  .fov = shot->pose.fov,
							  .size = shot->pose.size,
							  .track = shot->track};
}

void updateSequencePlayers(Scene& ioScene, const float iSeconds) {
	OWL_PROFILE_FUNCTION()

	std::vector<entt::entity> players;
	for (const auto view = ioScene.registry.view<SequencePlayer>(); const auto handle: view) players.push_back(handle);
	for (const auto handle: players) {
		const Entity entity{handle, &ioScene};
		if (!entity || !entity.hasComponent<SequencePlayer>())
			continue;
		if (auto& player = entity.getComponent<SequencePlayer>(); !player.m_started) {
			player.m_started = true;
			if (player.playOnStart) {
				std::ignore = playSequence(entity);
				continue;
			}
		}
		if (const auto& player = entity.getComponent<SequencePlayer>(); player.m_playing && player.m_sequence)
			advance(entity, iSeconds);
	}
}

}// namespace owl::scene
