/**
 * @file SceneWorldOrigin.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/Scene.h"

#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/ai/BehaviourTreeInstance.h"
#include "scene/component/BehaviourTreeAgent.h"
#include "scene/component/Drift.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/LightFlicker.h"
#include "scene/component/NavMeshVolume.h"
#include "scene/component/Oscillator.h"
#include "scene/component/ParticleEmitter.h"
#include "scene/component/Patrol.h"
#include "scene/component/SteeringAgent.h"
#include "scene/component/Trail.h"
#include "scene/component/Transform.h"
#include "scene/component/WalkArea.h"
#include "scene/component/WorldPosition.h"

namespace owl::scene {

namespace {

auto toDouble(const math::vec3& iVector) -> math::vec3d {
	return {static_cast<double>(iVector.x()), static_cast<double>(iVector.y()), static_cast<double>(iVector.z())};
}

auto toFloat(const math::vec3d& iVector) -> math::vec3 {
	return {static_cast<float>(iVector.x()), static_cast<float>(iVector.y()), static_cast<float>(iVector.z())};
}

auto isRoot(const entt::registry& iRegistry, const entt::entity iHandle) -> bool {
	const auto* hierarchy = iRegistry.try_get<component::Hierarchy>(iHandle);
	return hierarchy == nullptr || hierarchy->parentId == core::UUID{0};
}

void markSynced(component::WorldPosition& ioWorld, const math::vec3& iLocal) {
	ioWorld.m_synced = true;
	ioWorld.m_syncedLocal = iLocal;
	ioWorld.m_syncedPosition = ioWorld.position;
}

// The rest positions the behaviour systems move an entity around are in the space of its Transform: they move with it.
void shiftBehaviourAnchors(entt::registry& ioRegistry, const entt::entity iHandle, const math::vec3& iShift) {
	if (auto* oscillator = ioRegistry.try_get<component::Oscillator>(iHandle); oscillator != nullptr)
		oscillator->m_basePosition = oscillator->m_basePosition - iShift;
	if (auto* flicker = ioRegistry.try_get<component::LightFlicker>(iHandle); flicker != nullptr)
		flicker->m_basePosition = flicker->m_basePosition - iShift;
	if (auto* patrol = ioRegistry.try_get<component::Patrol>(iHandle); patrol != nullptr) {
		patrol->m_basePosition = patrol->m_basePosition - iShift;
		patrol->m_position = patrol->m_position - iShift;
		patrol->m_target = patrol->m_target - iShift;
		for (auto& waypoint: patrol->waypoints) waypoint = waypoint - iShift;
	}
	if (auto* drift = ioRegistry.try_get<component::Drift>(iHandle); drift != nullptr) {
		// The camera moves back by the same shift, which already carries the share of it the drift follows.
		const float kept = 1.f - drift->cameraFollow;
		drift->m_basePosition = drift->m_basePosition - math::vec3{iShift.x() * kept, iShift.y() * kept, iShift.z()};
	}
}

// The world points the AI keeps (goals, paths, baked meshes, last positions) belong to no Transform: every entity's move.
void shiftAiPoints(entt::registry& ioRegistry, const math::vec3& iShift) {
	for (auto&& [handle, volume]: ioRegistry.view<component::NavMeshVolume>().each()) {
		const math::vec3 shift = volume.toMeshSpace(iShift);
		for (auto& vertex: volume.vertices) vertex = vertex - shift;
	}
	for (auto&& [handle, agent]: ioRegistry.view<component::NavAgent>().each()) {
		agent.m_target = agent.m_target - iShift;
		for (auto& corner: agent.m_path) corner.position = corner.position - iShift;
	}
	for (auto&& [handle, obstacle]: ioRegistry.view<component::NavObstacle>().each())
		obstacle.m_lastPosition = obstacle.m_lastPosition - iShift;
	const math::vec2 planeShift{iShift.x(), iShift.y()};
	for (auto&& [handle, walker]: ioRegistry.view<component::WalkAgent>().each())
		for (auto& corner: walker.m_path) corner = corner - planeShift;
	for (auto&& [handle, agent]: ioRegistry.view<component::SteeringAgent>().each()) {
		agent.targetPoint = agent.targetPoint - iShift;
		agent.m_targetLast = agent.m_targetLast - iShift;
	}
	for (auto&& [handle, agent]: ioRegistry.view<component::BehaviourTreeAgent>().each())
		if (agent.m_instance)
			agent.m_instance->shiftOrigin(iShift);
}

}// namespace

void Scene::syncWorldPositions() {
	OWL_PROFILE_FUNCTION()

	for (const auto view = registry.view<component::WorldPosition, component::Transform>(); const auto handle: view) {
		if (!isRoot(registry, handle))
			continue;
		auto& world = view.get<component::WorldPosition>(handle);
		auto& local = view.get<component::Transform>(handle).transform.translation();
		if (world.m_synced && local != world.m_syncedLocal)
			world.position = m_worldOrigin + toDouble(local);
		else if (!world.m_synced || world.position != world.m_syncedPosition)
			local = toFloat(world.position - m_worldOrigin);
		markSynced(world, local);
	}
}

void Scene::shiftWorldOrigin(const math::vec3d& iOrigin) {
	OWL_PROFILE_FUNCTION()

	if (iOrigin == m_worldOrigin)
		return;
	syncWorldPositions();
	const math::vec3 shift = toFloat(iOrigin - m_worldOrigin);
	m_worldOrigin = iOrigin;
	for (const auto view = registry.view<component::Transform>(); const auto handle: view) {
		if (!isRoot(registry, handle))
			continue;
		auto& local = view.get<component::Transform>(handle).transform.translation();
		if (auto* world = registry.try_get<component::WorldPosition>(handle); world != nullptr) {
			local = toFloat(world->position - m_worldOrigin);
			markSynced(*world, local);
		} else {
			local = local - shift;
		}
		shiftBehaviourAnchors(registry, handle, shift);
	}
	for (const auto [handle, emitter]: registry.view<component::ParticleEmitter>().each()) {
		if (emitter.m_slot.system)
			emitter.m_slot.system->shiftOrigin(shift);
	}
	for (const auto [handle, trail]: registry.view<component::Trail>().each()) trail.shiftOrigin(shift);
	shiftAiPoints(registry, shift);
	physics::PhysicCommand::shiftOrigin(*this, shift);
}

template<>
OWL_API void Scene::onComponentAdded<component::WorldPosition>(const Entity& iEntity,
															   component::WorldPosition& ioComponent) {
	// A fresh component takes the place of the entity, so adding it never moves the entity.
	if (ioComponent.m_synced || ioComponent.position != math::vec3d{0.0, 0.0, 0.0} ||
		!iEntity.hasComponent<component::Transform>())
		return;
	const auto& local = iEntity.getComponent<component::Transform>().transform.translation();
	ioComponent.position = m_worldOrigin + toDouble(local);
	markSynced(ioComponent, local);
}

}// namespace owl::scene
