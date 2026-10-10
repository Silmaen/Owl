/**
 * @file PrefabSerializer.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/PrefabSerializer.h"

#include "EntityLinkMigration.h"
#include "scene/SceneSerializer.h"

#include "core/FormatVersionYaml.h"
#include "core/Serializer.h"
#include "core/SerializerImpl.h"
#include "platform/AtomicFile.h"
#include "scene/ComponentRegistry.h"
#include "scene/Entity.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/PrefabLink.h"
#include "scene/component/componentsSerialization.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <queue>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::scene {

namespace {

// 1 -> 2: entity links reference their target by UUID.
constexpr std::array<core::MigrationStep, 1> g_prefabMigrations{&bindEntityLinksByName};
constexpr core::DocumentFormat g_prefabFormat{.name = "Prefab", .migrations = g_prefabMigrations};

auto readText(const std::filesystem::path& iFilepath) -> std::string {
	std::ifstream file(iFilepath, std::ios::binary);
	if (!file.is_open())
		throw core::YamlError(std::format("Prefab: Cannot open '{}'.", iFilepath.string()));
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Throws core::YamlError when the file cannot be read or parsed.
auto loadPrefabDocument(const std::filesystem::path& iFilepath) -> shared<const core::YamlDocument> {
	auto text = readText(iFilepath);
	auto document = mkShared<const core::YamlDocument>(text, iFilepath.string());
	if (const auto root = document->getRoot(); !root.isMap() || !root["Prefab"]) {
		OWL_CORE_ERROR("Prefab: '{}' is not a prefab file (missing 'Prefab' key).", iFilepath.string())
		return nullptr;
	}
	if (canReadWithoutMigration(document->getRoot(), g_prefabFormat))
		return document;
	if (!upgradeDocumentText(g_prefabFormat, text, iFilepath.string()))
		return nullptr;
	return mkShared<const core::YamlDocument>(text, iFilepath.string());
}

void serializeEntity(const core::Serializer& iOut, const Entity& iEntity) {
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "Entity" << YAML::Value << iEntity.getUUID();
	component::serializeComponents(iEntity, iOut);
	iOut.getImpl()->emitter << YAML::EndMap;
}

auto collectSubtreeBFS(const Entity& iRoot, const Scene& iScene) -> std::vector<Entity> {
	std::vector<Entity> result;
	std::queue<Entity> queue;
	queue.push(iRoot);
	while (!queue.empty()) {
		const auto current = queue.front();
		queue.pop();
		result.push_back(current);
		for (const auto& child: iScene.getChildren(current)) queue.push(child);
	}
	return result;
}

void deserializeEntity(const shared<Scene>& ioScene, const core::YamlNode& iNode, const core::Serializer& iScratch) {
	const auto uuid = iNode["Entity"].as<uint64_t>();
	std::string name;
	if (const auto tagComponent = iNode["Tag"]; tagComponent)
		name = tagComponent["tag"].as<std::string>();
	Entity entity = ioScene->createEntityWithUUID(core::UUID{uuid}, name);
	component::deserializeComponents(entity, iNode, iScratch, true);
}

// Loads the entities of a prefab into a new scene, with their canonical UUIDs.
auto loadEntities(const core::YamlNode& iEntities) -> shared<Scene> {
	auto scene = mkShared<Scene>();
	const core::Serializer scratch;
	for (const auto entityNode: iEntities) deserializeEntity(scene, entityNode, scratch);
	scene->rebuildHierarchyChildren();
	return scene;
}

void copyOptionalComponents(Entity& ioDst, const Entity& iSrc) {
	for (const auto& desc: ComponentRegistry::getAll()) {
		if (!desc.optional || !desc.has(iSrc))
			continue;
		if (desc.copiable) {
			desc.copy(ioDst, iSrc);
			continue;
		}
		// A component that refuses copies still goes through its serialization, as when the prefab is loaded.
		const core::Serializer sOut;
		sOut.getImpl()->emitter << YAML::BeginMap;
		desc.serialize(iSrc, sOut);
		sOut.getImpl()->emitter << YAML::EndMap;
		const core::YamlDocument document{sOut.getImpl()->emitter.c_str()};
		const core::Serializer sIn;
		sIn.getImpl()->node = document.getRoot()[desc.key];
		desc.deserialize(ioDst, sIn);
	}
}

void linkInstanceChildren(const std::vector<Entity>& iInstance) {
	std::unordered_map<core::UUID, Entity> byUuid;
	byUuid.reserve(iInstance.size());
	for (const auto& entity: iInstance) byUuid.emplace(entity.getUUID(), entity);
	for (const auto& entity: iInstance) {
		const auto parentId = entity.getComponent<component::Hierarchy>().parentId;
		if (const auto parent = byUuid.find(parentId); parent != byUuid.end())
			parent->second.getComponent<component::Hierarchy>().childrenIds.push_back(entity.getUUID());
	}
}

}// namespace

auto PrefabSerializer::serializeToString(const Entity& iRootEntity, const Scene& iScene, const std::string& iPrefabName)
		-> std::string {
	const auto entities = collectSubtreeBFS(iRootEntity, iScene);

	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	sOut.getImpl()->emitter << YAML::Key << "Prefab" << YAML::Value << iPrefabName;
	emitFormatVersion(sOut.getImpl()->emitter, g_prefabFormat);
	sOut.getImpl()->emitter << YAML::Key << "Version" << YAML::Value << 1;
	sOut.getImpl()->emitter << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
	for (const auto& entity: entities) serializeEntity(sOut, entity);
	sOut.getImpl()->emitter << YAML::EndSeq;
	sOut.getImpl()->emitter << YAML::EndMap;
	return sOut.getImpl()->emitter.c_str();
}

auto PrefabSerializer::format() -> const core::DocumentFormat& { return g_prefabFormat; }

auto PrefabSerializer::serialize(const Entity& iRootEntity, const Scene& iScene, const std::filesystem::path& iFilepath,
								 const std::string& iPrefabName) -> bool {
	if (const auto written = platform::writeFileAtomic(iFilepath, serializeToString(iRootEntity, iScene, iPrefabName));
		!written) {
		OWL_CORE_ERROR("Prefab: Unable to save '{}': {}.", iFilepath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto PrefabSerializer::instantiate(const std::filesystem::path& iFilepath, const shared<Scene>& ioScene,
								   const std::string& iAssetRelativePath) -> Entity {
	try {
		const auto document = loadPrefabDocument(iFilepath);
		if (!document) {
			OWL_CORE_ERROR("Prefab: Unable to instantiate '{}'.", iFilepath.string())
			return {};
		}
		const auto root = document->getRoot();
		const uint32_t version = root["Version"] ? root["Version"].as<uint32_t>() : 1;
		const auto entitiesNode = root["Entities"];
		if (!entitiesNode) {
			OWL_CORE_ERROR("Prefab {} has no entities.", iFilepath.string())
			return {};
		}

		// Phase 1: Load all entities into a temporary scene with their canonical UUIDs.
		const auto tempScene = loadEntities(entitiesNode);

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

		std::unordered_map<uint64_t, uint64_t> uuidRemap;
		std::vector<component::PrefabLink::UuidMapEntry> uuidMapping;
		std::vector<Entity> instanceEntities;
		instanceEntities.reserve(orderedEntities.size());
		for (const auto& srcEntity: orderedEntities) {
			const auto canonicalUuid = static_cast<uint64_t>(srcEntity.getUUID());
			const auto newEntity = ioScene->createEntity(srcEntity.getName());
			const auto instanceUuid = static_cast<uint64_t>(newEntity.getUUID());
			uuidRemap[canonicalUuid] = instanceUuid;
			uuidMapping.push_back({.instanceUuid = instanceUuid, .canonicalUuid = canonicalUuid});
			instanceEntities.push_back(newEntity);
		}
		// Phase 4: Copy components from temp entities to new entities and remap hierarchy UUIDs.
		for (size_t index = 0; index < orderedEntities.size(); ++index) {
			const auto& srcEntity = orderedEntities[index];
			auto dstEntity = instanceEntities[index];
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
				if (const auto it = uuidRemap.find(static_cast<uint64_t>(srcHier.parentId)); it != uuidRemap.end())
					dstHier.parentId = core::UUID{it->second};
			}
			copyOptionalComponents(dstEntity, srcEntity);
		}

		// Phase 5: Link the instance children (the instance root stays a scene root), relink the links between members.
		linkInstanceChildren(instanceEntities);
		std::unordered_map<core::UUID, core::UUID> linkRemap;
		for (const auto& [canonical, instance]: uuidRemap)
			linkRemap.emplace(core::UUID{canonical}, core::UUID{instance});
		Scene::remapEntityLinks(instanceEntities, linkRemap);
		// Phase 6: Add PrefabLink to the root entity.
		auto instanceRoot = instanceEntities.front();
		if (instanceRoot) {
			auto& prefabLink = instanceRoot.addComponent<component::PrefabLink>();
			prefabLink.prefabAssetPath =
					iAssetRelativePath.empty() ? iFilepath.filename().string() : iAssetRelativePath;
			prefabLink.syncedVersion = version;
			prefabLink.uuidMapping = std::move(uuidMapping);
		}
		return instanceRoot;
	} catch (...) {
		OWL_CORE_ERROR("Unable to instantiate prefab from file {}.", iFilepath.string())
		return {};
	}
}

auto PrefabSerializer::readInfo(const std::filesystem::path& iFilepath) -> std::optional<PrefabInfo> {
	try {
		const auto document = loadPrefabDocument(iFilepath);
		if (!document) {
			OWL_CORE_WARN("Prefab: Cannot read info from '{}'.", iFilepath.string())
			return std::nullopt;
		}
		const auto data = document->getRoot();
		PrefabInfo info;
		info.name = data["Prefab"].as<std::string>();
		info.version = data["Version"] ? data["Version"].as<uint32_t>() : 1;
		if (auto entities = data["Entities"]; entities)
			info.entityCount = entities.size();
		return info;
	} catch (const std::exception& e) {
		OWL_CORE_WARN("Prefab: cannot read info from '{}': {}.", iFilepath.string(), e.what())
		return std::nullopt;
	} catch (...) {
		OWL_CORE_WARN("Prefab: cannot read info from '{}' (unknown error).", iFilepath.string())
		return std::nullopt;
	}
}

namespace {
struct LoadedPrefab {
	shared<Scene> scene;
	uint32_t version = 0;
};

auto loadPrefabToTempScene(const std::filesystem::path& iFilepath) -> std::optional<LoadedPrefab> {
	try {
		const auto document = loadPrefabDocument(iFilepath);
		if (!document) {
			OWL_CORE_WARN("Prefab: Cannot load '{}'.", iFilepath.string())
			return std::nullopt;
		}
		const auto root = document->getRoot();
		const uint32_t version = root["Version"] ? root["Version"].as<uint32_t>() : 1;
		const auto entitiesNode = root["Entities"];
		if (!entitiesNode) {
			OWL_CORE_WARN("Prefab: '{}' has no 'Entities' section.", iFilepath.string())
			return std::nullopt;
		}
		return LoadedPrefab{.scene = loadEntities(entitiesNode), .version = version};
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("Prefab: failed to load '{}': {}.", iFilepath.string(), e.what())
		return std::nullopt;
	} catch (...) {
		OWL_CORE_ERROR("Prefab: failed to load '{}' (unknown error).", iFilepath.string())
		return std::nullopt;
	}
}

auto findPrefabRoot(const Scene& iPrefabScene) -> Entity {
	for (const auto& entity: iPrefabScene.getAllEntities()) {
		if (entity.getComponent<component::Hierarchy>().parentId == core::UUID{0})
			return entity;
	}
	return {};
}

auto loadEntityNode(const Entity& iEntity) -> YAML::Node {
	return YAML::Load(SceneSerializer::serializeEntityToString(iEntity));
}

auto collectComponentKeys(const YAML::Node& iFirst, const YAML::Node& iSecond) -> std::vector<std::string> {
	std::vector<std::string> keys;
	for (const auto& node: {iFirst, iSecond}) {
		for (const auto& entry: node) {
			auto key = entry.first.as<std::string>();
			if (key != "Entity" && key != component::Hierarchy::key() && std::ranges::find(keys, key) == keys.end())
				keys.push_back(std::move(key));
		}
	}
	return keys;
}

auto mergeEntityYaml(const core::UUID iUuid, const YAML::Node& iPrefab, const YAML::Node& iInstance,
					 const std::function<auto(const std::string&)->bool>& iKeepInstance) -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Entity" << YAML::Value << static_cast<uint64_t>(iUuid);
	for (const auto& key: collectComponentKeys(iPrefab, iInstance)) {
		const auto& source = iKeepInstance(key) ? iInstance : iPrefab;
		if (const auto node = source[key]; node)
			out << YAML::Key << key << YAML::Value << node;
	}
	out << YAML::EndMap;
	return out.c_str();
}

auto isInstancePlacement(const bool iIsRoot, const std::string& iComponentKey) -> bool {
	return iIsRoot && iComponentKey == component::Transform::key();
}

auto keepsInstanceState(const component::PrefabLink& iLink, const uint64_t iCanonicalUuid, const std::string& iKey,
						const bool iIsRoot) -> bool {
	return iKey == component::PrefabLink::key() || isInstancePlacement(iIsRoot, iKey) ||
		   iLink.isOverridden(iCanonicalUuid, iKey);
}

auto createInstanceEntity(Scene& ioScene, const Entity& iPrefabEntity,
						  const std::unordered_map<uint64_t, uint64_t>& iCanonicalToInstance) -> Entity {
	const auto entity = ioScene.createEntity(iPrefabEntity.getName());
	const auto canonicalParent = static_cast<uint64_t>(iPrefabEntity.getComponent<component::Hierarchy>().parentId);
	const auto found = iCanonicalToInstance.find(canonicalParent);
	if (found == iCanonicalToInstance.end())
		return entity;
	if (const auto parent = ioScene.findEntityByUUID(core::UUID{found->second}); parent) {
		entity.getComponent<component::Hierarchy>().parentId = parent.getUUID();
		parent.getComponent<component::Hierarchy>().childrenIds.push_back(entity.getUUID());
	}
	return entity;
}

auto pruneOverrides(const std::vector<std::string>& iOverrides,
					const std::vector<component::PrefabLink::UuidMapEntry>& iMapping) -> std::vector<std::string> {
	std::vector<std::string> kept;
	for (const auto& key: iOverrides) {
		const bool alive = std::ranges::any_of(iMapping, [&key](const auto& iEntry) -> bool {
			return key.starts_with(std::format("{}:", iEntry.canonicalUuid));
		});
		if (alive)
			kept.push_back(key);
	}
	return kept;
}

}// namespace

auto PrefabSerializer::applyToInstance(const std::filesystem::path& iFilepath, const Entity& ioInstanceRoot,
									   Scene& ioScene) -> bool {
	if (!ioInstanceRoot || !ioInstanceRoot.hasComponent<component::PrefabLink>()) {
		OWL_CORE_WARN("Prefab: Cannot apply a prefab to an entity that is not a prefab instance root.")
		return false;
	}
	const auto loaded = loadPrefabToTempScene(iFilepath);
	if (!loaded.has_value()) {
		OWL_CORE_ERROR("Prefab: Failed to load '{}' for update.", iFilepath.string())
		return false;
	}
	const auto prefabRoot = findPrefabRoot(*loaded->scene);
	if (!prefabRoot) {
		OWL_CORE_ERROR("Prefab: '{}' has no root entity.", iFilepath.string())
		return false;
	}

	// NOLINTNEXTLINE(performance-unnecessary-copy-initialization) the link is rewritten at the end.
	const auto link = ioInstanceRoot.getComponent<component::PrefabLink>();
	const auto rootUuid = static_cast<uint64_t>(ioInstanceRoot.getUUID());
	std::unordered_map<uint64_t, uint64_t> canonicalToInstance;
	for (const auto& [instanceUuid, canonicalUuid]: link.uuidMapping) {
		if (instanceUuid != rootUuid && ioScene.findEntityByUUID(core::UUID{instanceUuid}))
			canonicalToInstance.emplace(canonicalUuid, instanceUuid);
	}
	canonicalToInstance[static_cast<uint64_t>(prefabRoot.getUUID())] = rootUuid;

	std::vector<component::PrefabLink::UuidMapEntry> mapping;
	for (const auto& prefabEntity: collectSubtreeBFS(prefabRoot, *loaded->scene)) {
		const auto canonicalUuid = static_cast<uint64_t>(prefabEntity.getUUID());
		const bool isRoot = prefabEntity == prefabRoot;
		Entity instanceEntity;
		if (const auto found = canonicalToInstance.find(canonicalUuid); found != canonicalToInstance.end())
			instanceEntity = ioScene.findEntityByUUID(core::UUID{found->second});
		YAML::Node instanceNode{YAML::NodeType::Map};
		if (instanceEntity) {
			instanceNode = loadEntityNode(instanceEntity);
		} else {
			instanceEntity = createInstanceEntity(ioScene, prefabEntity, canonicalToInstance);
			canonicalToInstance[canonicalUuid] = static_cast<uint64_t>(instanceEntity.getUUID());
		}
		const auto merged = mergeEntityYaml(
				instanceEntity.getUUID(), loadEntityNode(prefabEntity), instanceNode,
				[&](const std::string& iKey) -> bool { return keepsInstanceState(link, canonicalUuid, iKey, isRoot); });
		if (!SceneSerializer::applyEntityFromString(instanceEntity, merged))
			OWL_CORE_WARN("Prefab: Entity {} of '{}' could not be updated.", canonicalUuid, iFilepath.string())
		mapping.push_back(
				{.instanceUuid = static_cast<uint64_t>(instanceEntity.getUUID()), .canonicalUuid = canonicalUuid});
	}

	for (const auto& entry: std::views::reverse(link.uuidMapping)) {
		if (entry.instanceUuid == rootUuid ||
			std::ranges::find(mapping, entry.instanceUuid, &component::PrefabLink::UuidMapEntry::instanceUuid) !=
					mapping.end())
			continue;
		if (auto stale = ioScene.findEntityByUUID(core::UUID{entry.instanceUuid}); stale)
			ioScene.destroyEntity(stale);
	}

	std::unordered_map<core::UUID, core::UUID> linkRemap;
	std::vector<Entity> instanceEntities;
	for (const auto& [instanceUuid, canonicalUuid]: mapping) {
		linkRemap.emplace(core::UUID{canonicalUuid}, core::UUID{instanceUuid});
		instanceEntities.push_back(ioScene.findEntityByUUID(core::UUID{instanceUuid}));
	}
	Scene::remapEntityLinks(instanceEntities, linkRemap);

	auto& updatedLink = ioInstanceRoot.getComponent<component::PrefabLink>();
	updatedLink.overriddenComponents = pruneOverrides(link.overriddenComponents, mapping);
	updatedLink.uuidMapping = std::move(mapping);
	updatedLink.syncedVersion = loaded->version;
	return true;
}

auto PrefabSerializer::revertInstance(const std::filesystem::path& iFilepath, const Entity& ioInstanceRoot,
									  Scene& ioScene) -> bool {
	if (!ioInstanceRoot || !ioInstanceRoot.hasComponent<component::PrefabLink>()) {
		OWL_CORE_WARN("Prefab: Cannot revert an entity that is not a prefab instance root.")
		return false;
	}
	auto& overrides = ioInstanceRoot.getComponent<component::PrefabLink>().overriddenComponents;
	auto saved = std::move(overrides);
	overrides.clear();
	if (applyToInstance(iFilepath, ioInstanceRoot, ioScene))
		return true;
	OWL_CORE_WARN("Prefab: Revert of '{}' failed, overrides kept.", iFilepath.string())
	ioInstanceRoot.getComponent<component::PrefabLink>().overriddenComponents = std::move(saved);
	return false;
}

auto PrefabSerializer::revertComponent(const std::filesystem::path& iFilepath, const Entity& iInstanceRoot,
									   const Entity& iEntity, const std::string& iComponentKey) -> bool {
	if (!iInstanceRoot || !iInstanceRoot.hasComponent<component::PrefabLink>() || !iEntity) {
		OWL_CORE_WARN("Prefab: Cannot revert a component outside a prefab instance.")
		return false;
	}
	if (iComponentKey == component::Hierarchy::key() || iComponentKey == component::PrefabLink::key() ||
		isInstancePlacement(iEntity == iInstanceRoot, iComponentKey)) {
		OWL_CORE_WARN("Prefab: Component {} belongs to the instance and cannot be reverted.", iComponentKey)
		return false;
	}
	auto& link = iInstanceRoot.getComponent<component::PrefabLink>();
	const auto canonicalUuid = link.findCanonicalUuid(static_cast<uint64_t>(iEntity.getUUID()));
	if (!canonicalUuid.has_value()) {
		OWL_CORE_WARN("Prefab: Entity {} is not part of the instance.", static_cast<uint64_t>(iEntity.getUUID()))
		return false;
	}
	const auto loaded = loadPrefabToTempScene(iFilepath);
	if (!loaded.has_value()) {
		OWL_CORE_ERROR("Prefab: Failed to load '{}' for a component revert.", iFilepath.string())
		return false;
	}
	const auto prefabEntity = loaded->scene->findEntityByUUID(core::UUID{*canonicalUuid});
	if (!prefabEntity) {
		OWL_CORE_WARN("Prefab: Entity {} no longer exists in '{}'.", *canonicalUuid, iFilepath.string())
		return false;
	}
	const auto merged =
			mergeEntityYaml(iEntity.getUUID(), loadEntityNode(prefabEntity), loadEntityNode(iEntity),
							[&iComponentKey](const std::string& iKey) -> bool { return iKey != iComponentKey; });
	if (!SceneSerializer::applyEntityFromString(iEntity, merged)) {
		OWL_CORE_WARN("Prefab: Component {} could not be reverted.", iComponentKey)
		return false;
	}
	link.clearOverride(*canonicalUuid, iComponentKey);
	return true;
}

auto PrefabSerializer::findInstanceRoot(const Entity& iEntity, const Scene& iScene) -> Entity {
	if (!iEntity)
		return {};
	const auto uuid = static_cast<uint64_t>(iEntity.getUUID());
	Entity current = iEntity;
	while (current) {
		if (current.hasComponent<component::PrefabLink>() &&
			current.getComponent<component::PrefabLink>().findCanonicalUuid(uuid).has_value())
			return current;
		const auto parentId = current.getComponent<component::Hierarchy>().parentId;
		if (parentId == core::UUID{0})
			break;
		current = iScene.findEntityByUUID(parentId);
	}
	return {};
}

auto PrefabSerializer::recordOverrides(const Entity& iEntity, const Scene& iScene, const std::string& iBeforeYaml)
		-> bool {
	const auto root = findInstanceRoot(iEntity, iScene);
	if (!root)
		return false;
	auto& link = root.getComponent<component::PrefabLink>();
	const auto canonicalUuid = link.findCanonicalUuid(static_cast<uint64_t>(iEntity.getUUID())).value_or(0);
	try {
		const auto before = YAML::Load(iBeforeYaml);
		const auto after = loadEntityNode(iEntity);
		bool recorded = false;
		for (const auto& key: collectComponentKeys(before, after)) {
			if (key == component::PrefabLink::key() || isInstancePlacement(root == iEntity, key))
				continue;
			const auto previous = before[key];
			const auto current = after[key];
			const bool changed = static_cast<bool>(previous) != static_cast<bool>(current) ||
								 (previous && YAML::Dump(previous) != YAML::Dump(current));
			if (changed && link.setOverridden(canonicalUuid, key))
				recorded = true;
		}
		return recorded;
	} catch (const std::exception& e) {
		OWL_CORE_WARN("Prefab: Cannot compare entity states to record overrides: {}.", e.what())
		return false;
	}
}

}// namespace owl::scene
