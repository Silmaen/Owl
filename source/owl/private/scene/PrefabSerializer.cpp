/**
 * @file PrefabSerializer.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/PrefabSerializer.h"
#include "scene/SceneSerializer.h"

#include "core/Serializer.h"
#include "core/SerializerImpl.h"
#include "scene/Entity.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/PrefabLink.h"
#include "scene/component/componentsSerialization.h"

#include <fstream>
#include <queue>

namespace owl::scene {

namespace {
/**
 * @brief
 *  Serialize a single entity to YAML (same format as SceneSerializer).
 */
void serializeEntity(const core::Serializer& iOut, const Entity& iEntity) {
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "Entity" << YAML::Value << iEntity.getUUID();
	serializeComponents(iEntity, iOut, component::SerializableComponents{});
	iOut.getImpl()->emitter << YAML::EndMap;
}

/**
 * @brief
 *  Collect entities in BFS order starting from iRoot.
 */
auto collectSubtreeBFS(const Entity& iRoot, const Scene& iScene) -> std::vector<Entity> {
	std::vector<Entity> result;
	std::queue<Entity> queue;
	queue.push(iRoot);
	while (!queue.empty()) {
		const auto current = queue.front();
		queue.pop();
		result.push_back(current);
		for (const auto& child: iScene.getChildren(current))
			queue.push(child);
	}
	return result;
}

/**
 * @brief
 *  Deserialize a single entity into a scene (reused from SceneSerializer pattern).
 */
void deserializeEntity(const shared<Scene>& ioScene, const core::Serializer& iNode) {
	auto uuid = iNode.getImpl()->node["Entity"].as<uint64_t>();
	std::string name;
	if (auto tagComponent = iNode.getImpl()->node["Tag"]; tagComponent)
		name = tagComponent["tag"].as<std::string>();

	const core::Serializer sNode;
	Entity entity = ioScene->createEntityWithUUID(core::UUID{uuid}, name);
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Transform"]); sNode.getImpl()->node) {
		auto& comp = entity.getComponent<component::Transform>();
		comp.deserialize(sNode);
	}
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Visibility"]); sNode.getImpl()->node) {
		auto& comp = entity.getComponent<component::Visibility>();
		comp.deserialize(sNode);
	}
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Hierarchy"]); sNode.getImpl()->node) {
		auto& comp = entity.getComponent<component::Hierarchy>();
		comp.deserialize(sNode);
	}
	deserializeComponents(entity, iNode, component::OptionalComponents{});
}

}// namespace

auto PrefabSerializer::serializeToString(const Entity& iRootEntity, const Scene& iScene,
										 const std::string& iPrefabName) -> std::string {
	const auto entities = collectSubtreeBFS(iRootEntity, iScene);

	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	sOut.getImpl()->emitter << YAML::Key << "Prefab" << YAML::Value << iPrefabName;
	sOut.getImpl()->emitter << YAML::Key << "Version" << YAML::Value << 1;
	sOut.getImpl()->emitter << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
	for (const auto& entity: entities)
		serializeEntity(sOut, entity);
	sOut.getImpl()->emitter << YAML::EndSeq;
	sOut.getImpl()->emitter << YAML::EndMap;
	return sOut.getImpl()->emitter.c_str();
}

void PrefabSerializer::serialize(const Entity& iRootEntity, const Scene& iScene,
								 const std::filesystem::path& iFilepath, const std::string& iPrefabName) {
	std::ofstream fileOut(iFilepath);
	fileOut << serializeToString(iRootEntity, iScene, iPrefabName);
	fileOut.close();
}

auto PrefabSerializer::instantiate(const std::filesystem::path& iFilepath, const shared<Scene>& ioScene,
								   const std::string& iAssetRelativePath) -> Entity {
	try {
		const core::Serializer sData;
		sData.getImpl()->node.reset(YAML::LoadFile(iFilepath.string()));
		if (!sData.getImpl()->node["Prefab"]) {
			OWL_CORE_ERROR("File {} is not a prefab.", iFilepath.string())
			return {};
		}
		const uint32_t version = sData.getImpl()->node["Version"]
										 ? sData.getImpl()->node["Version"].as<uint32_t>()
										 : 1;

		const auto entitiesNode = sData.getImpl()->node["Entities"];
		if (!entitiesNode) {
			OWL_CORE_ERROR("Prefab {} has no entities.", iFilepath.string())
			return {};
		}

		// Phase 1: Load all entities into a temporary scene with their canonical UUIDs.
		auto tempScene = mkShared<Scene>();
		for (auto entityNode: entitiesNode) {
			const core::Serializer sEntity;
			sEntity.getImpl()->node.reset(entityNode);

			deserializeEntity(tempScene, sEntity);
		}
		tempScene->rebuildHierarchyChildren();

		// Phase 2: Collect canonical entities in BFS order.
		const auto tempEntities = tempScene->getAllEntities();
		// Find the root entity (parentId == 0).
		Entity tempRoot;
		for (const auto& entity: tempEntities) {
			if (entity.getComponent<component::Hierarchy>().parentId == core::UUID{0}) {
				tempRoot = entity;
				break;
			}
		}
		if (!tempRoot) {
			OWL_CORE_ERROR("Prefab {} has no root entity.", iFilepath.string())
			return {};
		}
		const auto orderedEntities = collectSubtreeBFS(tempRoot, *tempScene);

		// Phase 3: Create new entities in the target scene with new UUIDs.
		// Build canonical->instance UUID mapping.
		std::unordered_map<uint64_t, uint64_t> uuidRemap;
		std::vector<component::PrefabLink::UuidMapEntry> uuidMapping;
		for (const auto& srcEntity: orderedEntities) {
			const auto canonicalUuid = static_cast<uint64_t>(srcEntity.getUUID());
			const auto newEntity = ioScene->createEntity(srcEntity.getName());
			const auto instanceUuid = static_cast<uint64_t>(newEntity.getUUID());
			uuidRemap[canonicalUuid] = instanceUuid;
			uuidMapping.push_back({.instanceUuid = instanceUuid, .canonicalUuid = canonicalUuid});
		}
		// Phase 4: Copy components from temp entities to new entities and remap hierarchy UUIDs.
		for (const auto& srcEntity: orderedEntities) {
			const auto canonicalUuid = static_cast<uint64_t>(srcEntity.getUUID());
			auto dstEntity = ioScene->findEntityByUUID(core::UUID{uuidRemap[canonicalUuid]});
			if (!dstEntity)
				continue;
			// Copy transform.
			dstEntity.getComponent<component::Transform>().transform =
					srcEntity.getComponent<component::Transform>().transform;
			// Copy visibility.
			if (srcEntity.hasComponent<component::Visibility>())
				dstEntity.getComponent<component::Visibility>() = srcEntity.getComponent<component::Visibility>();
			// Copy hierarchy with remapped UUIDs.
			auto& dstHier = dstEntity.getComponent<component::Hierarchy>();
			const auto& srcHier = srcEntity.getComponent<component::Hierarchy>();
			if (srcHier.parentId != core::UUID{0}) {
				if (const auto it = uuidRemap.find(static_cast<uint64_t>(srcHier.parentId));
					it != uuidRemap.end())
					dstHier.parentId = core::UUID{it->second};
			}
			// childrenIds will be rebuilt.
			// Copy optional components via serialization round-trip.
			// Serialize the source entity, then deserialize optional components onto the dest.
			const auto entityYaml = SceneSerializer::serializeEntityToString(srcEntity);
			const core::Serializer sEntity;
			sEntity.getImpl()->node.reset(YAML::Load(entityYaml));

			component::deserializeComponents(dstEntity, sEntity, component::OptionalComponents{});
		}

		// Phase 5: Rebuild hierarchy in the target scene.
		ioScene->rebuildHierarchyChildren();
		// Phase 6: Add PrefabLink to the root entity.
		auto instanceRoot = ioScene->findEntityByUUID(core::UUID{uuidRemap[static_cast<uint64_t>(tempRoot.getUUID())]});
		if (instanceRoot) {
			auto& prefabLink = instanceRoot.addComponent<component::PrefabLink>();
			prefabLink.prefabAssetPath =
					iAssetRelativePath.empty() ? iFilepath.filename().string() : iAssetRelativePath;
			prefabLink.syncedVersion = version;
			prefabLink.uuidMapping = std::move(uuidMapping);
		}
		return instanceRoot;
	} catch (...) {
		OWL_CORE_ERROR("Unable to instantiate prefab from file {}", iFilepath.string())
		return {};
	}
}

auto PrefabSerializer::readInfo(const std::filesystem::path& iFilepath) -> std::optional<PrefabInfo> {
	try {
		const auto data = YAML::LoadFile(iFilepath.string());
		if (!data["Prefab"])
			return std::nullopt;
		PrefabInfo info;
		info.name = data["Prefab"].as<std::string>();
		info.version = data["Version"] ? data["Version"].as<uint32_t>() : 1;
		if (auto entities = data["Entities"]; entities)
			info.entityCount = entities.size();
		return info;
	} catch (const std::exception& e) {
		OWL_CORE_WARN("Prefab: cannot read info from '{}': {}", iFilepath.string(), e.what())
		return std::nullopt;
	} catch (...) {
		OWL_CORE_WARN("Prefab: cannot read info from '{}' (unknown error).", iFilepath.string())
		return std::nullopt;
	}
}

namespace {
/// Load a prefab file into a temporary scene and return it with the version.
struct LoadedPrefab {
	/// The temporary scene containing the prefab entities.
	shared<Scene> scene;
	/// Prefab version from the file header.
	uint32_t version = 0;
};

auto loadPrefabToTempScene(const std::filesystem::path& iFilepath) -> std::optional<LoadedPrefab> {
	try {
		const core::Serializer sData;
		sData.getImpl()->node.reset(YAML::LoadFile(iFilepath.string()));
		if (!sData.getImpl()->node["Prefab"]) {
			OWL_CORE_WARN("Prefab: '{}' is not a valid prefab file (missing 'Prefab' key).", iFilepath.string())
			return std::nullopt;
		}
		const uint32_t version =
				sData.getImpl()->node["Version"] ? sData.getImpl()->node["Version"].as<uint32_t>() : 1;
		const auto entitiesNode = sData.getImpl()->node["Entities"];
		if (!entitiesNode) {
			OWL_CORE_WARN("Prefab: '{}' has no 'Entities' section.", iFilepath.string())
			return std::nullopt;
		}
		auto tempScene = mkShared<Scene>();
		for (auto entityNode: entitiesNode) {
			const core::Serializer sEntity;
			sEntity.getImpl()->node.reset(entityNode);
			deserializeEntity(tempScene, sEntity);
		}
		tempScene->rebuildHierarchyChildren();
		return LoadedPrefab{.scene = std::move(tempScene), .version = version};
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("Prefab: failed to load '{}': {}", iFilepath.string(), e.what())
		return std::nullopt;
	} catch (...) {
		OWL_CORE_ERROR("Prefab: failed to load '{}' (unknown error).", iFilepath.string())
		return std::nullopt;
	}
}

/**
 * @brief
 *  Build a map from canonical UUID to YAML string for each entity in the prefab.
 */
auto buildCanonicalYamlMap(const Scene& iPrefabScene) -> std::unordered_map<uint64_t, std::string> {
	std::unordered_map<uint64_t, std::string> result;
	for (const auto& entity: iPrefabScene.getAllEntities()) {
		if (!entity)
			continue;
		result[static_cast<uint64_t>(entity.getUUID())] = SceneSerializer::serializeEntityToString(entity);
	}
	return result;
}

/**
 * @brief
 *  Check if a specific component key is overridden for a given canonical UUID.
 */
auto isOverridden(const std::vector<std::string>& iOverrides, uint64_t iCanonicalUuid,
				  const std::string& iComponentKey) -> bool {
	const auto key = std::format("{}:{}", iCanonicalUuid, iComponentKey);
	return std::ranges::find(iOverrides, key) != iOverrides.end();
}

}// namespace

auto PrefabSerializer::applyToInstance(const std::filesystem::path& iFilepath, const Entity& ioInstanceRoot,
									   Scene& ioScene) -> bool {
	if (!ioInstanceRoot || !ioInstanceRoot.hasComponent<component::PrefabLink>())
		return false;
	const auto loaded = loadPrefabToTempScene(iFilepath);
	if (!loaded.has_value()) {
		OWL_CORE_ERROR("Failed to load prefab for update: {}", iFilepath.string())
		return false;
	}

	// Copy PrefabLink by value — the original component will be destroyed during the loop below.
	// NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
	const auto prefabLink = ioInstanceRoot.getComponent<component::PrefabLink>();
	const auto canonicalYaml = buildCanonicalYamlMap(*loaded->scene);
	// For each entity in the UUID mapping, update non-overridden components.
	for (const auto& [instanceUuid, canonicalUuid]: prefabLink.uuidMapping) {
		auto instanceEntity = ioScene.findEntityByUUID(core::UUID{instanceUuid});
		if (!instanceEntity)
			continue;
		const auto it = canonicalYaml.find(canonicalUuid);
		if (it == canonicalYaml.end())
			continue;
		// Parse the prefab entity YAML to get individual component nodes.
		const auto prefabNode = YAML::Load(it->second);
		// Serialize the instance entity for comparison.
		const auto instanceYaml = SceneSerializer::serializeEntityToString(instanceEntity);
		const auto instanceNode = YAML::Load(instanceYaml);
		// Rebuild the entity from prefab YAML, keeping overridden parts from instance.
		YAML::Emitter mergedEmitter;
		mergedEmitter << YAML::BeginMap;
		mergedEmitter << YAML::Key << "Entity" << YAML::Value << instanceUuid;
		// Write components: use prefab for non-overridden, instance for overridden.
		for (const auto& compEntry: prefabNode) {
			const auto compKey = compEntry.first.as<std::string>();
			if (compKey == "Entity")
				continue;
			if (compKey == "Tag") {
				// Preserve instance name.
				if (instanceNode["Tag"])
					mergedEmitter << YAML::Key << "Tag" << YAML::Value << instanceNode["Tag"];
				else
					mergedEmitter << YAML::Key << "Tag" << YAML::Value << compEntry.second;
				continue;
			}
			if (compKey == "Hierarchy") {
				// Preserve instance hierarchy.
				if (instanceNode["Hierarchy"])
					mergedEmitter << YAML::Key << "Hierarchy" << YAML::Value << instanceNode["Hierarchy"];
				continue;
			}
			if (compKey == "PrefabLink") {
				continue;// PrefabLink is managed separately, not from the prefab file.
			}
			if (isOverridden(prefabLink.overriddenComponents, canonicalUuid, compKey)) {
				// Use instance version.
				if (instanceNode[compKey])
					mergedEmitter << YAML::Key << compKey << YAML::Value << instanceNode[compKey];
			} else {
				// Use prefab version.
				mergedEmitter << YAML::Key << compKey << YAML::Value << compEntry.second;
			}
		}
		// Add instance-only components not in prefab (e.g., components added to instance only).
		for (const auto& instEntry: instanceNode) {
			const auto compKey = instEntry.first.as<std::string>();
			if (compKey == "Entity" || compKey == "PrefabLink")
				continue;
			if (!prefabNode[compKey])
				mergedEmitter << YAML::Key << compKey << YAML::Value << instEntry.second;
		}
		mergedEmitter << YAML::EndMap;
		// Replace the instance entity.
		ioScene.destroyEntity(instanceEntity);
		const auto sceneRef = shared<Scene>(shared<Scene>{}, &ioScene);
		std::ignore = SceneSerializer::deserializeEntityFromString(sceneRef, mergedEmitter.c_str());
	}
	// Rebuild hierarchy and restore PrefabLink.
	ioScene.rebuildHierarchyChildren();
	// Re-add PrefabLink (it was on the root, which was destroyed and recreated).
	auto newRoot = ioScene.findEntityByUUID(core::UUID{prefabLink.uuidMapping[0].instanceUuid});
	if (newRoot) {
		auto& newLink = newRoot.hasComponent<component::PrefabLink>()
								? newRoot.getComponent<component::PrefabLink>()
								: newRoot.addComponent<component::PrefabLink>();
		newLink = prefabLink;
		newLink.syncedVersion = loaded->version;
	}
	return true;
}

auto PrefabSerializer::revertInstance(const std::filesystem::path& iFilepath, const Entity& ioInstanceRoot,
									  Scene& ioScene) -> bool {
	if (!ioInstanceRoot || !ioInstanceRoot.hasComponent<component::PrefabLink>())
		return false;

	// Clear all overrides and apply.
	auto& prefabLink = ioInstanceRoot.getComponent<component::PrefabLink>();
	prefabLink.overriddenComponents.clear();
	return applyToInstance(iFilepath, ioInstanceRoot, ioScene);
}

}// namespace owl::scene
