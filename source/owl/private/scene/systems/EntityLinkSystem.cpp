/**
 * @file EntityLinkSystem.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"

#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

namespace owl::scene::systems {

void updateEntityLinks(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const auto rescanTag = [&ioScene](component::EntityLink& ioLink) -> void {
		ioLink.linkedEntity = {};
		for (const auto view = ioScene.registry.view<component::Tag>(); const auto entity: view) {
			if (view.get<component::Tag>(entity).tag == ioLink.linkedEntityName) {
				ioLink.linkedEntity = {entity, &ioScene};
				return;
			}
		}
	};
	const auto applyLocalFromWorld = [&ioScene](component::Transform& ioTransform, const Entity& iHost,
												const math::Transform& iLinkedWorld) -> void {
		const auto& [parentId, childrenIds] = iHost.getComponent<component::Hierarchy>();
		if (parentId == core::UUID{0}) {
			ioTransform.transform.translation() = iLinkedWorld.translation();
			return;
		}
		const Entity parent = ioScene.findEntityByUUID(parentId);
		if (!parent) {
			ioTransform.transform.translation() = iLinkedWorld.translation();
			return;
		}
		const math::mat4 parentWorldInv = math::inverse(ioScene.getWorldTransform(parent)());
		const math::vec4 localPos =
				parentWorldInv * math::vec4{iLinkedWorld.translation().x(), iLinkedWorld.translation().y(),
											iLinkedWorld.translation().z(), 1.0f};
		ioTransform.transform.translation().x() = localPos.x();
		ioTransform.transform.translation().y() = localPos.y();
		ioTransform.transform.translation().z() = localPos.z();
	};
	for (const auto view = ioScene.registry.view<component::Transform, component::EntityLink>();
		 const auto entity: view) {
		const Entity host{entity, &ioScene};
		if (!ioScene.isEffectivelyVisible(host, /*iEditorMode=*/false))
			continue;
		auto [transform, link] = view.get<component::Transform, component::EntityLink>(entity);
		if (link.linkedEntityName.empty())
			continue;
		if (!link.linkedEntity || link.linkedEntity.getComponent<component::Tag>().tag != link.linkedEntityName)
			rescanTag(link);
		if (!link.linkedEntity) {
			if (!link.wasUnresolvedReported)
				OWL_CORE_WARN("Scene: Entity link of '{}' lost its target '{}', link ignored.", host.getName(),
							  link.linkedEntityName)
			link.wasUnresolvedReported = true;
			continue;
		}
		link.wasUnresolvedReported = false;
		applyLocalFromWorld(transform, host, ioScene.getWorldTransform(link.linkedEntity));
	}
}

}// namespace owl::scene::systems
