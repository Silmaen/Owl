/**
 * @file PrefabLink.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "core/UUID.h"

#include <vector>

namespace owl::scene::component {
/**
 * @brief
 *  Component linking an entity subtree to a source prefab file.
 *
 * Placed on the root entity of a prefab instance. Tracks the source `.owlprefab`
 * asset path, the version last synced, and a UUID mapping from instance UUIDs to
 * canonical prefab UUIDs (for override detection and prefab update propagation).
 */
struct OWL_API PrefabLink {
	/// Relative path to the .owlprefab asset (relative to project assets root).
	std::string prefabAssetPath;
	/// Version of the prefab when this instance was last synced.
	uint32_t syncedVersion = 0;

	/// Mapping entry: instance entity UUID -> canonical prefab UUID.
	struct UuidMapEntry {
		/// Instance entity UUID.
		uint64_t instanceUuid = 0;
		/// Canonical prefab UUID (from the .owlprefab file).
		uint64_t canonicalUuid = 0;
	};
	/// Mapping from instance entity UUIDs to canonical prefab UUIDs.
	std::vector<UuidMapEntry> uuidMapping;

	/// Per-component override keys. Format: "canonicalUUID:ComponentKey".
	/// Components in this list are preserved during prefab update propagation.
	std::vector<std::string> overriddenComponents;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Prefab Link"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "PrefabLink"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
