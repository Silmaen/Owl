/**
 * @file PrefabSerializer.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"
#include "core/FormatVersion.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace owl::scene {
/**
 * @brief
 *  Serializer for prefab files (.owlprefab).
 *
 * A prefab is a reusable entity subtree template. The serializer saves/loads
 * entity subtrees and handles UUID remapping during instantiation.
 */
class OWL_API PrefabSerializer final {
public:
	PrefabSerializer() = delete;

	/**
	 * @brief
	 *  Format descriptor of the prefab files (`.owlprefab`), with its migration chain.
	 *
	 * Distinct from the prefab `Version`, which counts the edits of one prefab's content.
	 * @return The prefab format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Serialize an entity subtree to a .owlprefab file, atomically (see `platform::writeFileAtomic`).
	 * @param[in] iRootEntity The root entity of the subtree.
	 * @param[in] iScene The scene containing the entity.
	 * @param[in] iFilepath Path to write the .owlprefab file.
	 * @param[in] iPrefabName Human-readable name for the prefab.
	 * @return True on success; on failure the previous file is left untouched.
	 */
	[[nodiscard]] static auto serialize(const Entity& iRootEntity, const Scene& iScene,
										const std::filesystem::path& iFilepath, const std::string& iPrefabName) -> bool;

	/**
	 * @brief
	 *  Serialize an entity subtree to a YAML string.
	 * @param[in] iRootEntity The root entity of the subtree.
	 * @param[in] iScene The scene containing the entity.
	 * @param[in] iPrefabName Human-readable name for the prefab.
	 * @return The YAML string.
	 */
	[[nodiscard]] static auto serializeToString(const Entity& iRootEntity, const Scene& iScene,
												const std::string& iPrefabName) -> std::string;

	/**
	 * @brief
	 *  Instantiate a prefab from file into a scene.
	 *
	 * Creates new entities with new UUIDs and adds a PrefabLink component
	 * to the root entity with the UUID mapping and asset path. An older format
	 * version is migrated on the fly, a newer one is refused.
	 * @param[in] iFilepath Path to the .owlprefab file.
	 * @param[in] ioScene The scene to instantiate into.
	 * @param[in] iAssetRelativePath Relative path for the PrefabLink (e.g., "prefabs/enemy.owlprefab").
	 * @return The instantiated root entity, or an invalid entity on failure.
	 */
	static auto instantiate(const std::filesystem::path& iFilepath, const shared<Scene>& ioScene,
							const std::string& iAssetRelativePath = {}) -> Entity;

	/// Metadata read from a prefab file header.
	struct PrefabInfo {
		/// Prefab display name.
		std::string name;
		/// Prefab version number.
		uint32_t version = 0;
		/// Number of entities in the prefab.
		size_t entityCount = 0;
	};

	/**
	 * @brief
	 *  Read prefab metadata without fully instantiating.
	 * @param[in] iFilepath Path to the .owlprefab file.
	 * @return The metadata, or nullopt on failure.
	 */
	[[nodiscard]] static auto readInfo(const std::filesystem::path& iFilepath) -> std::optional<PrefabInfo>;

	/**
	 * @brief
	 *  Apply prefab updates to an existing instance, in place.
	 *
	 * Entities are matched through `PrefabLink::uuidMapping` and updated with
	 * `SceneSerializer::applyEntityFromString`: handles, UUIDs and the instance hierarchy survive.
	 * For each mapped entity, every component takes the prefab state except the overridden ones
	 * (listed in `PrefabLink::overriddenComponents`) and the Transform of the instance root, which
	 * always keeps the instance placement. Entities new in the prefab are created under their
	 * mapped parent; mapped entities gone from the prefab are destroyed (their children move to
	 * the grandparent, keeping their world position). Entities added to the instance only are kept.
	 * @param[in] iFilepath Path to the .owlprefab file.
	 * @param[in,out] ioInstanceRoot Root entity of the prefab instance.
	 * @param[in,out] ioScene The scene.
	 * @return True if the update was successful.
	 */
	[[nodiscard]] static auto applyToInstance(const std::filesystem::path& iFilepath, const Entity& ioInstanceRoot,
											  Scene& ioScene) -> bool;

	/**
	 * @brief
	 *  Revert all overrides on an instance, making it match the prefab.
	 *
	 * Clears `PrefabLink::overriddenComponents` then applies the prefab: only the Transform of the
	 * instance root (its placement) and the entities added to the instance only are kept.
	 * @param[in] iFilepath Path to the .owlprefab file.
	 * @param[in,out] ioInstanceRoot Root entity of the prefab instance.
	 * @param[in,out] ioScene The scene.
	 * @return True if the revert was successful.
	 */
	[[nodiscard]] static auto revertInstance(const std::filesystem::path& iFilepath, const Entity& ioInstanceRoot,
											 Scene& ioScene) -> bool;

	/**
	 * @brief
	 *  Revert one component of an instance entity to its prefab state and clear its override.
	 *
	 * A component absent from the prefab entity is removed. The Transform of the instance root is
	 * never reverted (it is the instance placement).
	 * @param[in] iFilepath Path to the .owlprefab file.
	 * @param[in] iInstanceRoot Root entity of the prefab instance.
	 * @param[in] iEntity Instance entity holding the component (the root or one of its mapped entities).
	 * @param[in] iComponentKey YAML key of the component (e.g. "SpriteRenderer").
	 * @return True if the component was reverted.
	 */
	[[nodiscard]] static auto revertComponent(const std::filesystem::path& iFilepath, const Entity& iInstanceRoot,
											  const Entity& iEntity, const std::string& iComponentKey) -> bool;

	/**
	 * @brief
	 *  Find the root of the prefab instance an entity belongs to.
	 * @param[in] iEntity The entity (an instance root or one of its mapped entities).
	 * @param[in] iScene The scene holding the entity.
	 * @return The nearest ancestor-or-self carrying a `PrefabLink` that maps the entity, or an invalid entity.
	 */
	[[nodiscard]] static auto findInstanceRoot(const Entity& iEntity, const Scene& iScene) -> Entity;

	/**
	 * @brief
	 *  Mark as overridden every component of an instance entity that changed since a snapshot.
	 *
	 * Compares the entity with its YAML before the edit, component by component; added, removed and
	 * modified components are recorded in the instance `PrefabLink`. `Hierarchy`, `PrefabLink` and
	 * the Transform of the instance root are never recorded. Entities outside any instance, or added
	 * to an instance only, are ignored.
	 * @param[in] iEntity The edited entity.
	 * @param[in] iScene The scene holding the entity.
	 * @param[in] iBeforeYaml The entity YAML before the edit (`SceneSerializer::serializeEntityToString`).
	 * @return True when at least one new override was recorded.
	 */
	static auto recordOverrides(const Entity& iEntity, const Scene& iScene, const std::string& iBeforeYaml) -> bool;
};

}// namespace owl::scene
