/**
 * @file EntityHandle.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "EntityHandle.h"

#include "scene/Scene.h"
#include "scene/component/components.h"

#include <cstdint>
#include <optional>

namespace owl::script {

namespace {

auto handleOf(const entt::entity iEntity, const bool iHud) -> uint64_t {
	return uint64_t{entt::to_integral(iEntity)} + 1 + (iHud ? g_HudEntityHandleBit : 0);
}

}// namespace

auto getEntityHandle(const scene::Scene& iScene, const scene::Entity& iEntity) -> uint64_t {
	if (!iEntity)
		return 0;
	return handleOf(static_cast<entt::entity>(iEntity), iEntity.getScene() != &iScene);
}

auto getEntityHandle(const scene::Scene& iScene, const core::UUID iUuid) -> uint64_t {
	if (iUuid == 0)
		return 0;
	if (const auto entity = iScene.findEntityByUUID(iUuid); entity)
		return getEntityHandle(iScene, entity);
	if (const auto hud = iScene.getHud(); hud)
		if (const auto entity = hud->findEntityByUUID(iUuid); entity)
			return handleOf(static_cast<entt::entity>(entity), true);
	return 0;
}

auto resolveEntityHandle(scene::Scene& iScene, const uint64_t iHandle) -> std::optional<scene::Entity> {
	const auto index = iHandle & (g_HudEntityHandleBit - 1);
	if (index == 0 || (iHandle & ~((g_HudEntityHandleBit << 1U) - 1)) != 0)
		return std::nullopt;
	auto* scene = &iScene;
	const auto hud = (iHandle & g_HudEntityHandleBit) != 0 ? iScene.getHud() : nullptr;
	if ((iHandle & g_HudEntityHandleBit) != 0) {
		if (!hud)
			return std::nullopt;
		scene = hud.get();
	}
	const scene::Entity entity{static_cast<entt::entity>(static_cast<entt::id_type>(index - 1)), scene};
	if (!entity || !entity.hasComponent<scene::component::ID>())
		return std::nullopt;
	return entity;
}

auto getEntityHandleUuid(scene::Scene& iScene, const uint64_t iHandle) -> core::UUID {
	const auto entity = resolveEntityHandle(iScene, iHandle);
	return entity ? entity->getUUID() : core::UUID{0};
}

}// namespace owl::script
