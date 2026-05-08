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

#include <queue>

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
	auto sceneRef = shared<scene::Scene>(shared<scene::Scene>{}, &ioScene);
	std::ignore = scene::SceneSerializer::deserializeEntityFromString(sceneRef, yamlData);
	return ioScene.findEntityByUUID(uuid);
}

auto SubtreeSnapshot::capture(const scene::Entity& iRootEntity, const scene::Scene& iScene) -> SubtreeSnapshot {
	SubtreeSnapshot snapshot;

	// BFS traversal of the subtree.
	std::queue<scene::Entity> queue;
	queue.push(iRootEntity);

	while (!queue.empty()) {
		const auto current = queue.front();
		queue.pop();

		snapshot.entities.push_back(EntitySnapshot::capture(current));

		// Parent UUID: 0 for the subtree root, actual parent for others.
		core::UUID parentUuid{0};
		if (current != iRootEntity && current.hasComponent<scene::component::Hierarchy>())
			parentUuid = current.getComponent<scene::component::Hierarchy>().parentId;
		snapshot.parentUuids.push_back(parentUuid);

		for (const auto& child: iScene.getChildren(current)) queue.push(child);
	}
	return snapshot;
}

auto SubtreeSnapshot::restore(scene::Scene& ioScene) const -> scene::Entity {
	if (entities.empty())
		return {};

	// Restore all entities.
	for (const auto& entitySnap: entities) entitySnap.restore(ioScene);

	// Rebuild parent-child relationships for non-root entities.
	for (size_t i = 1; i < entities.size(); ++i) {
		if (parentUuids[i] != core::UUID{0}) {
			auto child = ioScene.findEntityByUUID(entities[i].uuid);
			auto parent = ioScene.findEntityByUUID(parentUuids[i]);
			if (child && parent)
				ioScene.setParent(child, parent);
		}
	}

	return ioScene.findEntityByUUID(entities[0].uuid);
}

}// namespace owl::nest
