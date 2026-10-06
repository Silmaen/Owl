/**
 * @file EntitySnapshot.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EntitySnapshot.h"

#include <scene/SceneSerializer.h>
#include <scene/component/Hierarchy.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <queue>
#include <tuple>

namespace owl::nest {

auto EntitySnapshot::capture(const scene::Entity& iEntity) -> EntitySnapshot {
	EntitySnapshot snapshot;
	snapshot.uuid = iEntity.getUUID();
	snapshot.yamlData = scene::SceneSerializer::serializeEntityToString(iEntity);
	return snapshot;
}

auto EntitySnapshot::restore(scene::Scene& ioScene) const -> scene::Entity {
	if (yamlData.empty())
		return {};
	if (const auto existing = ioScene.findEntityByUUID(uuid); existing) {
		std::ignore = scene::SceneSerializer::applyEntityFromString(existing, yamlData);
		return existing;
	}
	const auto sceneRef = shared<scene::Scene>(shared<scene::Scene>{}, &ioScene);
	std::ignore = scene::SceneSerializer::deserializeEntityFromString(sceneRef, yamlData);
	return ioScene.findEntityByUUID(uuid);
}

auto HierarchySlot::capture(const scene::Entity& iEntity, const scene::Scene& iScene) -> HierarchySlot {
	HierarchySlot slot;
	slot.parentUuid = iEntity.getComponent<scene::component::Hierarchy>().parentId;
	if (const auto parent = iScene.findEntityByUUID(slot.parentUuid); parent) {
		const auto& siblings = parent.getComponent<scene::component::Hierarchy>().childrenIds;
		slot.siblingIndex =
				static_cast<size_t>(std::distance(siblings.begin(), std::ranges::find(siblings, iEntity.getUUID())));
	}
	return slot;
}

void HierarchySlot::restore(const scene::Entity& iEntity, scene::Scene& ioScene) const {
	const auto uuid = iEntity.getUUID();
	auto& hierarchy = iEntity.getComponent<scene::component::Hierarchy>();
	if (const auto oldParent = ioScene.findEntityByUUID(hierarchy.parentId); oldParent)
		std::erase(oldParent.getComponent<scene::component::Hierarchy>().childrenIds, uuid);
	const auto parent = ioScene.findEntityByUUID(parentUuid);
	if (!parent) {
		hierarchy.parentId = core::UUID{0};
		return;
	}
	hierarchy.parentId = parentUuid;
	auto& siblings = parent.getComponent<scene::component::Hierarchy>().childrenIds;
	const auto index = std::min(siblingIndex, siblings.size());
	siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(index), uuid);
}

auto SubtreeSnapshot::capture(const scene::Entity& iRootEntity, const scene::Scene& iScene) -> SubtreeSnapshot {
	SubtreeSnapshot snapshot;
	std::queue<scene::Entity> queue;
	queue.push(iRootEntity);
	while (!queue.empty()) {
		const auto current = queue.front();
		queue.pop();
		snapshot.entities.push_back(EntitySnapshot::capture(current));
		core::UUID parentUuid{0};
		if (current != iRootEntity)
			parentUuid = current.getComponent<scene::component::Hierarchy>().parentId;
		snapshot.parentUuids.push_back(parentUuid);
		for (const auto& child: iScene.getChildren(current)) queue.push(child);
	}
	return snapshot;
}

auto SubtreeSnapshot::restore(scene::Scene& ioScene) const -> scene::Entity {
	if (entities.empty())
		return {};
	for (size_t i = 0; i < entities.size(); ++i) {
		if (const auto existing = ioScene.findEntityByUUID(entities[i].uuid);
			existing && i > 0 && existing.getComponent<scene::component::Hierarchy>().parentId != parentUuids[i])
			HierarchySlot{.parentUuid = parentUuids[i], .siblingIndex = std::numeric_limits<size_t>::max()}.restore(
					existing, ioScene);
		entities[i].restore(ioScene);
	}
	for (const auto& entitySnap: entities) {
		const auto parent = ioScene.findEntityByUUID(entitySnap.uuid);
		if (!parent)
			continue;
		auto& children = parent.getComponent<scene::component::Hierarchy>().childrenIds;
		std::vector<core::UUID> ordered;
		for (size_t i = 1; i < entities.size(); ++i) {
			if (parentUuids[i] == entitySnap.uuid && std::ranges::find(children, entities[i].uuid) != children.end())
				ordered.push_back(entities[i].uuid);
		}
		for (const auto child: children) {
			if (std::ranges::find(ordered, child) == ordered.end())
				ordered.push_back(child);
		}
		children = std::move(ordered);
	}
	return ioScene.findEntityByUUID(entities[0].uuid);
}

}// namespace owl::nest
