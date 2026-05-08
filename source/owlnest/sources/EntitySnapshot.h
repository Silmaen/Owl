/**
 * @file EntitySnapshot.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest {
/**
 * @brief
 *  Captures the complete state of a single entity (UUID, name, all components)
 *        as a YAML string. Used by undo commands to snapshot/restore entities.
 */
struct EntitySnapshot {
	/// The entity's UUID.
	core::UUID uuid{0};
	/// Serialized YAML data (entity + all components).
	std::string yamlData;

	/**
	 * @brief
	 *  Capture all data from an entity.
	 * @param[in] iEntity The entity to snapshot.
	 * @return The snapshot.
	 */
	[[nodiscard]] static auto capture(const scene::Entity& iEntity) -> EntitySnapshot;

	/**
	 * @brief
	 *  Restore an entity into a scene from this snapshot.
	 * @param[in,out] ioScene The scene to create the entity in.
	 * @return The restored entity.
	 */
	auto restore(scene::Scene& ioScene) const -> scene::Entity;
};

/**
 * @brief
 *  Captures a subtree of entities (parent + all descendants).
 */
struct SubtreeSnapshot {
	/// Snapshots in BFS order (root first).
	std::vector<EntitySnapshot> entities;
	/// Parent UUID for each entity (0 = root of subtree).
	std::vector<core::UUID> parentUuids;

	/**
	 * @brief
	 *  Capture a subtree rooted at the given entity.
	 * @param[in] iRootEntity The root entity.
	 * @param[in] iScene The scene.
	 * @return The subtree snapshot.
	 */
	[[nodiscard]] static auto capture(const scene::Entity& iRootEntity, const scene::Scene& iScene) -> SubtreeSnapshot;

	/**
	 * @brief
	 *  Restore the entire subtree into a scene.
	 * @param[in,out] ioScene The scene to create entities in.
	 * @return The restored root entity.
	 */
	auto restore(scene::Scene& ioScene) const -> scene::Entity;
};

}// namespace owl::nest
