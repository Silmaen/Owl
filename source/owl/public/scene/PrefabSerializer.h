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
	 *  Apply prefab updates to an existing instance.
	 *
	 * Non-overridden components are refreshed from the prefab file.
	 * Overridden components (listed in PrefabLink::overriddenComponents) are preserved.
	 * @param[in] iFilepath Path to the .owlprefab file.
	 * @param[in,out] ioInstanceRoot Root entity of the prefab instance.
	 * @param[in,out] ioScene The scene.
	 * @return True if the update was successful.
	 */
	[[nodiscard]] static auto applyToInstance(const std::filesystem::path& iFilepath, const Entity& ioInstanceRoot,
											  Scene& ioScene) -> bool;

	/**
	 * @brief
	 *  Revert all overrides on an instance, making it match the prefab exactly.
	 * @param[in] iFilepath Path to the .owlprefab file.
	 * @param[in,out] ioInstanceRoot Root entity of the prefab instance.
	 * @param[in,out] ioScene The scene.
	 * @return True if the revert was successful.
	 */
	[[nodiscard]] static auto revertInstance(const std::filesystem::path& iFilepath, const Entity& ioInstanceRoot,
											 Scene& ioScene) -> bool;
};

}// namespace owl::scene
