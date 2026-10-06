/**
 * @file EntitySnapshot.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include <cstddef>
#include <string>
#include <vector>

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
	 *
	 * When an entity with this UUID exists it is updated in place: its handle, its hierarchy
	 * links and the components the snapshot did not change are kept. Otherwise the entity is
	 * created and linked under the parent recorded in the snapshot.
	 * @param[in,out] ioScene The scene to restore the entity in.
	 * @return The restored entity.
	 */
	auto restore(scene::Scene& ioScene) const -> scene::Entity;
};

/**
 * @brief
 *  Position of an entity in the hierarchy: its parent and its index among that parent's children.
 */
struct HierarchySlot {
	/// Parent UUID (0 for a root entity).
	core::UUID parentUuid{0};
	/// Index among the parent's children.
	size_t siblingIndex = 0;

	/**
	 * @brief
	 *  Capture the slot an entity currently occupies.
	 * @param[in] iEntity The entity.
	 * @param[in] iScene The scene holding the entity.
	 * @return The slot.
	 */
	[[nodiscard]] static auto capture(const scene::Entity& iEntity, const scene::Scene& iScene) -> HierarchySlot;

	/**
	 * @brief
	 *  Move an entity back into this slot, leaving its local transform untouched.
	 *
	 * The index is clamped to the parent's children count; a missing parent makes the entity a root.
	 * @param[in] iEntity The entity to move.
	 * @param[in,out] ioScene The scene holding the entity.
	 */
	void restore(const scene::Entity& iEntity, scene::Scene& ioScene) const;
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
	 *
	 * Entities that still exist are updated in place, missing ones are created, and the parent links
	 * and sibling order inside the subtree are put back. The root keeps its current slot.
	 * @param[in,out] ioScene The scene to restore the entities in.
	 * @return The restored root entity.
	 */
	auto restore(scene::Scene& ioScene) const -> scene::Entity;
};

}// namespace owl::nest
