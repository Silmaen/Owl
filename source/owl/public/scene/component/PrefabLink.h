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

#include <cstdint>
#include <optional>
#include <string>
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

	/**
	 * Per-component override keys. Format: "canonicalUUID:ComponentKey".
	 * A listed component keeps its instance state (present, absent or modified) when the prefab is
	 * applied. The Transform of the instance root is always kept and never listed.
	 */
	std::vector<std::string> overriddenComponents;

	/**
	 * @brief
	 *  Build the override key of a component of a prefab entity.
	 * @param[in] iCanonicalUuid Canonical prefab UUID of the entity.
	 * @param[in] iComponentKey YAML key of the component (e.g. "SpriteRenderer").
	 * @return The key, formatted as "canonicalUUID:ComponentKey".
	 */
	[[nodiscard]] static auto overrideKey(uint64_t iCanonicalUuid, const std::string& iComponentKey) -> std::string;

	/**
	 * @brief
	 *  Find the canonical prefab UUID of an instance entity.
	 * @param[in] iInstanceUuid Instance entity UUID.
	 * @return The canonical UUID, or nullopt when the entity is not part of this instance.
	 */
	[[nodiscard]] auto findCanonicalUuid(uint64_t iInstanceUuid) const -> std::optional<uint64_t>;

	/**
	 * @brief
	 *  Check whether a component of a prefab entity is overridden on this instance.
	 * @param[in] iCanonicalUuid Canonical prefab UUID of the entity.
	 * @param[in] iComponentKey YAML key of the component.
	 * @return True when the component is overridden.
	 */
	[[nodiscard]] auto isOverridden(uint64_t iCanonicalUuid, const std::string& iComponentKey) const -> bool;

	/**
	 * @brief
	 *  Mark a component of a prefab entity as overridden.
	 * @param[in] iCanonicalUuid Canonical prefab UUID of the entity.
	 * @param[in] iComponentKey YAML key of the component.
	 * @return True when the override was added, false when it already existed.
	 */
	auto setOverridden(uint64_t iCanonicalUuid, const std::string& iComponentKey) -> bool;

	/**
	 * @brief
	 *  Remove the override mark of a component of a prefab entity.
	 * @param[in] iCanonicalUuid Canonical prefab UUID of the entity.
	 * @param[in] iComponentKey YAML key of the component.
	 * @return True when an override was removed.
	 */
	auto clearOverride(uint64_t iCanonicalUuid, const std::string& iComponentKey) -> bool;

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
