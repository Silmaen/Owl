/**
 * @file SceneWorldOrigin.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/Scene.h"

#include "scene/Entity.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/Transform.h"
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
	}
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
