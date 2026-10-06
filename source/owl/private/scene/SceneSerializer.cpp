/**
 * @file SceneSerializer.cpp
 * @author Silmaen
 * @date 27/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SceneSerializer.h"

#include "app/Application.h"
#include "core/FormatVersionYaml.h"
#include "core/Serializer.h"
#include "core/SerializerImpl.h"
#include "platform/AtomicFile.h"
#include "scene/Entity.h"
#include "scene/component/componentsSerialization.h"

#include <cstdint>
#include <exception>
#include <format>
#include <tuple>
#include <unordered_set>

namespace owl::scene {

auto describe(const SceneLoadError iError) -> std::string_view {
	switch (iError) {
		case SceneLoadError::FileUnreadable:
			return "the file cannot be read";
		case SceneLoadError::InvalidYaml:
			return "the file is not valid YAML";
		case SceneLoadError::NotAScene:
			return "the file is not a scene";
		case SceneLoadError::InvalidEntity:
			return "an entity is malformed";
		case SceneLoadError::InvalidFormatVersion:
			return describe(core::FormatError::InvalidVersion);
		case SceneLoadError::NewerFormatVersion:
			return describe(core::FormatError::NewerVersion);
		case SceneLoadError::MigrationFailed:
			return describe(core::FormatError::MigrationFailed);
	}
	return "unknown error";
}

SceneSerializer::SceneSerializer(const shared<Scene>& iScene) : mp_scene(iScene) {}

namespace {

constexpr std::array<core::MigrationStep, 0> g_sceneMigrations{};
constexpr core::DocumentFormat g_sceneFormat{.name = "Scene", .migrations = g_sceneMigrations};

auto toSceneLoadError(const core::FormatError iError) -> SceneLoadError {
	switch (iError) {
		case core::FormatError::InvalidVersion:
			return SceneLoadError::InvalidFormatVersion;
		case core::FormatError::NewerVersion:
			return SceneLoadError::NewerFormatVersion;
		case core::FormatError::MigrationFailed:
			return SceneLoadError::MigrationFailed;
	}
	return SceneLoadError::MigrationFailed;
}

void serializeEntity(const core::Serializer& iOut, const Entity& iEntity) {
	iOut.getImpl()->emitter << YAML::BeginMap;// Entity
	iOut.getImpl()->emitter << YAML::Key << "Entity" << YAML::Value << iEntity.getUUID();
	serializeComponents(iEntity, iOut, component::SerializableComponents{});
	iOut.getImpl()->emitter << YAML::EndMap;// Entity
}

auto createEntityFromNode(const shared<Scene>& ioScene, const core::Serializer& iNode, const uint64_t iUuid) -> Entity {
	std::string name;
	if (auto tagComponent = iNode.getImpl()->node["Tag"]; tagComponent && tagComponent["tag"])
		name = tagComponent["tag"].as<std::string>();
	OWL_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}.", iUuid, name)
	return ioScene->createEntityWithUUID(core::UUID{iUuid}, name);
}

void deserializeEntityComponents(Entity& ioEntity, const core::Serializer& iNode) {
	const core::Serializer sNode;
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Transform"]); sNode.getImpl()->node)
		ioEntity.getComponent<component::Transform>().deserialize(sNode);
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Visibility"]); sNode.getImpl()->node)
		ioEntity.getComponent<component::Visibility>().deserialize(sNode);
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Hierarchy"]); sNode.getImpl()->node)
		ioEntity.getComponent<component::Hierarchy>().deserialize(sNode);
	deserializeComponents(ioEntity, iNode, component::OptionalComponents{});
}

using SeenUuids = std::unordered_set<uint64_t>;

auto isValidEntityNode(const YAML::Node& iNode) -> bool {
	return iNode.IsMap() && iNode["Entity"] && iNode["Entity"].IsScalar();
}

auto uniqueUuid(const uint64_t iUuid, SeenUuids& ioSeen, const std::string& iSceneName) -> uint64_t {
	if (iUuid != 0 && ioSeen.insert(iUuid).second)
		return iUuid;
	const auto fresh = static_cast<uint64_t>(core::UUID{});
	OWL_CORE_WARN("SceneSerializer: Entity UUID {} duplicated or null in scene '{}', renamed to {}.", iUuid, iSceneName,
				  fresh)
	ioSeen.insert(fresh);
	return fresh;
}

void rollback(const shared<Scene>& ioScene, std::vector<Entity>& ioCreated) {
	// Detach first so destroyEntity neither walks nor rewires half-loaded parents.
	for (const auto& entity: ioCreated) {
		auto& hierarchy = entity.getComponent<component::Hierarchy>();
		hierarchy.parentId = core::UUID{0};
		hierarchy.childrenIds.clear();
	}
	for (auto& entity: ioCreated) ioScene->destroyEntity(entity);
	ioCreated.clear();
}

auto readFile(const std::filesystem::path& iFilepath) -> std::optional<std::vector<uint8_t>> {
	std::ifstream file(iFilepath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		OWL_CORE_ERROR("SceneSerializer: Unable to open scene file {}.", iFilepath.string())
		return std::nullopt;
	}
	const auto size = file.tellg();
	if (size < 0) {
		OWL_CORE_ERROR("SceneSerializer: Unable to read scene file {}.", iFilepath.string())
		return std::nullopt;
	}
	file.seekg(0);
	std::vector<uint8_t> bytes(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
		OWL_CORE_ERROR("SceneSerializer: Unable to read scene file {}.", iFilepath.string())
		return std::nullopt;
	}
	return bytes;
}

void serializePhysicsSettings(YAML::Emitter& ioEmitter, const physics::PhysicsSettings& iSettings) {
	if (iSettings == physics::PhysicsSettings{})
		return;
	ioEmitter << YAML::Key << "Physics" << YAML::Value << YAML::BeginMap;
	ioEmitter << YAML::Key << "tickRate" << YAML::Value << iSettings.tickRate;
	ioEmitter << YAML::Key << "maxStepsPerFrame" << YAML::Value << iSettings.maxStepsPerFrame;
	ioEmitter << YAML::Key << "solverSubSteps" << YAML::Value << iSettings.solverSubSteps;
	ioEmitter << YAML::Key << "interpolate" << YAML::Value << iSettings.interpolate;
	ioEmitter << YAML::Key << "workerCount" << YAML::Value << iSettings.workerCount;
	ioEmitter << YAML::EndMap;
}

auto deserializePhysicsSettings(const YAML::Node& iNode) -> physics::PhysicsSettings {
	physics::PhysicsSettings settings;
	if (!iNode || !iNode.IsMap())
		return settings;
	settings.tickRate = iNode["tickRate"].as<float>(settings.tickRate);
	settings.maxStepsPerFrame = iNode["maxStepsPerFrame"].as<uint32_t>(settings.maxStepsPerFrame);
	settings.solverSubSteps = iNode["solverSubSteps"].as<uint32_t>(settings.solverSubSteps);
	settings.interpolate = iNode["interpolate"].as<bool>(settings.interpolate);
	settings.workerCount = iNode["workerCount"].as<uint32_t>(settings.workerCount);
	return settings.clamped();
}

void linkIntoHierarchy(Scene& ioScene, const Entity& iEntity) {
	const core::UUID uuid = iEntity.getUUID();
	auto& hierarchy = iEntity.getComponent<component::Hierarchy>();
	if (hierarchy.parentId != core::UUID{0}) {
		if (const Entity parent = ioScene.findEntityByUUID(hierarchy.parentId); parent) {
			if (auto& siblings = parent.getComponent<component::Hierarchy>().childrenIds;
				std::ranges::find(siblings, uuid) == siblings.end())
				siblings.push_back(uuid);
		} else {
			OWL_CORE_WARN("SceneSerializer: Parent {} of entity {} not found, orphaning entity.",
						  static_cast<uint64_t>(hierarchy.parentId), static_cast<uint64_t>(uuid))
			hierarchy.parentId = core::UUID{0};
		}
	}
	for (const auto view = ioScene.registry.view<component::Hierarchy, component::ID>(); const auto handle: view) {
		if (view.get<component::Hierarchy>(handle).parentId == uuid &&
			std::ranges::find(hierarchy.childrenIds, view.get<component::ID>(handle).id) == hierarchy.childrenIds.end())
			hierarchy.childrenIds.push_back(view.get<component::ID>(handle).id);
	}
}

template<typename Component>
auto componentYaml(const Component& iComponent) -> std::string {
	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	iComponent.serialize(sOut);
	sOut.getImpl()->emitter << YAML::EndMap;
	return YAML::Dump(YAML::Load(sOut.getImpl()->emitter.c_str())[Component::key()]);
}

template<typename Component>
void applyComponent(Entity& ioEntity, const YAML::Node& iEntityNode, const bool iMandatory) {
	const auto node = iEntityNode[Component::key()];
	const bool present = ioEntity.hasComponent<Component>();
	if (!node) {
		if (iMandatory)
			ioEntity.getComponent<Component>() = Component{};
		else if (present)
			ioEntity.removeComponent<Component>();
		return;
	}
	if (present && componentYaml(ioEntity.getComponent<Component>()) == YAML::Dump(node))
		return;
	auto& comp = ioEntity.addOrReplaceComponent<Component>();
	const core::Serializer sNode;
	sNode.getImpl()->node.reset(node);
	comp.deserialize(sNode);
}

template<typename... Components>
void applyOptionalComponents(Entity& ioEntity, const YAML::Node& iEntityNode, const std::tuple<Components...>&) {
	(..., applyComponent<Components>(ioEntity, iEntityNode, false));
}

}// namespace

auto SceneSerializer::format() -> const core::DocumentFormat& { return g_sceneFormat; }

auto SceneSerializer::serializeToString() const -> std::string {
	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	sOut.getImpl()->emitter << YAML::Key << "Scene" << YAML::Value << "untitled";
	emitFormatVersion(sOut.getImpl()->emitter, g_sceneFormat);
	if (const auto& enabled = mp_scene->getEnabledRenderers(); !enabled.isEmpty()) {
		sOut.getImpl()->emitter << YAML::Key << "EnabledRenderers" << YAML::Value << enabled.toYaml();
	}
	serializePhysicsSettings(sOut.getImpl()->emitter, mp_scene->getPhysicsSettings());
	sOut.getImpl()->emitter << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
	for (const auto& entity: mp_scene->getAllEntities()) {
		if (!entity)
			continue;
		serializeEntity(sOut, entity);
	}
	sOut.getImpl()->emitter << YAML::EndSeq;
	sOut.getImpl()->emitter << YAML::EndMap;
	return sOut.getImpl()->emitter.c_str();
}

auto SceneSerializer::serialize(const std::filesystem::path& iFilepath) const -> bool {
	if (const auto written = platform::writeFileAtomic(iFilepath, serializeToString()); !written) {
		OWL_CORE_ERROR("SceneSerializer: Unable to save scene {}: {}.", iFilepath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto SceneSerializer::deserialize(const std::filesystem::path& iFilepath) const -> SceneLoadResult {
	const auto bytes = readFile(iFilepath);
	if (!bytes) {
		OWL_CORE_ERROR("SceneSerializer: Unable to load scene {}: {}.", iFilepath.string(),
					   describe(SceneLoadError::FileUnreadable))
		return unexpected{SceneLoadError::FileUnreadable};
	}
	return deserializeFromBuffer(*bytes, iFilepath.string());
}

auto SceneSerializer::deserializeFromBuffer(const std::vector<uint8_t>& iData, const std::string& iSourceName) const
		-> SceneLoadResult {
	const auto parsed = parseBuffer(iData, iSourceName);
	if (!parsed.valid) {
		OWL_CORE_ERROR("SceneSerializer: Unable to load scene from {}: {}.", iSourceName, parsed.error)
		return unexpected{parsed.failure};
	}
	return applyParsed(parsed);
}

auto SceneSerializer::parseBuffer(const std::vector<uint8_t>& iData, const std::string& iSourceName) -> ParsedScene {
	using clk = std::chrono::steady_clock;
	const auto t0 = clk::now();
	ParsedScene out;
	try {
		const std::string yamlStr(iData.begin(), iData.end());
		out.serializer = mkShared<core::Serializer>();
		out.serializer->getImpl()->node.reset(YAML::Load(yamlStr));
		auto& root = out.serializer->getImpl()->node;
		if (!root.IsMap() || !root["Scene"] || !root["Scene"].IsScalar()) {
			out.error = std::format("Buffer {} is not a scene", iSourceName);
			out.failure = SceneLoadError::NotAScene;
			out.serializer.reset();
			return out;
		}
		if (const auto version = upgradeYamlDocument(g_sceneFormat, root, iSourceName); !version) {
			out.error = std::format("Buffer {} cannot be read: {}", iSourceName, describe(version.error()));
			out.failure = toSceneLoadError(version.error());
			out.serializer.reset();
			return out;
		}
		if (const auto entities = root["Entities"]; entities && !entities.IsNull() && !entities.IsSequence()) {
			out.error = std::format("Buffer {} is not a scene", iSourceName);
			out.failure = SceneLoadError::NotAScene;
			out.serializer.reset();
			return out;
		}
		out.sceneName = root["Scene"].as<std::string>();
		out.valid = true;
	} catch (const std::exception& iEx) {
		out.error = iEx.what();
		out.failure = SceneLoadError::InvalidYaml;
		out.serializer.reset();
	} catch (...) {
		out.error = "unknown YAML parser failure";
		out.failure = SceneLoadError::InvalidYaml;
		out.serializer.reset();
	}
	const auto dur = std::chrono::duration<double, std::milli>{clk::now() - t0}.count();
	OWL_CORE_INFO("SceneSerializer::parseBuffer: '{}' {:.1f} ms ({} bytes, {}).", iSourceName, dur, iData.size(),
				  out.valid ? "ok" : out.error)
	return out;
}

auto SceneSerializer::applyParsed(const ParsedScene& iParsed) const -> SceneLoadResult {
	using clk = std::chrono::steady_clock;
	const auto t0 = clk::now();
	if (!iParsed.valid || !iParsed.serializer) {
		OWL_CORE_ERROR("SceneSerializer: Cannot apply an invalid parsed scene ({}).", iParsed.error)
		return unexpected{iParsed.valid ? SceneLoadError::InvalidYaml : iParsed.failure};
	}
	OWL_CORE_INFO("SceneSerializer::applyParsed: '{}' begin.", iParsed.sceneName)
	const auto& sData = *iParsed.serializer;
	const auto previousRenderers = mp_scene->getEnabledRenderers();
	const auto previousPhysics = mp_scene->getPhysicsSettings();
	std::vector<Entity> created;
	try {
		if (const auto enabled = sData.getImpl()->node["EnabledRenderers"]; enabled)
			mp_scene->getEnabledRenderers() = renderer::EnabledRenderersConfig::fromYaml(enabled);
		mp_scene->getPhysicsSettings() = deserializePhysicsSettings(sData.getImpl()->node["Physics"]);
		SeenUuids seen;
		if (auto entities = sData.getImpl()->node["Entities"]; entities && entities.IsSequence()) {
			created.reserve(entities.size());
			for (auto entity: entities) {
				if (!isValidEntityNode(entity)) {
					OWL_CORE_ERROR("SceneSerializer: Entry {} of scene '{}' is not an entity.", created.size(),
								   iParsed.sceneName)
					rollback(mp_scene, created);
					mp_scene->getEnabledRenderers() = previousRenderers;
					mp_scene->getPhysicsSettings() = previousPhysics;
					mp_scene->getPhysicsSettings() = previousPhysics;
					return unexpected{SceneLoadError::InvalidEntity};
				}
				const core::Serializer sEntity;
				sEntity.getImpl()->node.reset(entity);
				const auto uuid = uniqueUuid(entity["Entity"].as<uint64_t>(), seen, iParsed.sceneName);
				created.push_back(createEntityFromNode(mp_scene, sEntity, uuid));
				deserializeEntityComponents(created.back(), sEntity);
			}
		}
	} catch (const std::exception& iEx) {
		OWL_CORE_ERROR("SceneSerializer: Entity {} of scene '{}' is malformed: {}.", created.size(), iParsed.sceneName,
					   iEx.what())
		rollback(mp_scene, created);
		mp_scene->getEnabledRenderers() = previousRenderers;
		mp_scene->getPhysicsSettings() = previousPhysics;
		return unexpected{SceneLoadError::InvalidEntity};
	} catch (...) {
		OWL_CORE_ERROR("SceneSerializer: Entity {} of scene '{}' is malformed.", created.size(), iParsed.sceneName)
		rollback(mp_scene, created);
		mp_scene->getEnabledRenderers() = previousRenderers;
		mp_scene->getPhysicsSettings() = previousPhysics;
		return unexpected{SceneLoadError::InvalidEntity};
	}
	const auto hierStart = clk::now();
	mp_scene->rebuildHierarchyChildren();
	OWL_CORE_INFO("SceneSerializer::applyParsed: rebuildHierarchyChildren {:.1f} ms.",
				  std::chrono::duration<double, std::milli>{clk::now() - hierStart}.count())
	OWL_CORE_INFO("SceneSerializer::applyParsed: '{}' total {:.1f} ms ({} entities).", iParsed.sceneName,
				  std::chrono::duration<double, std::milli>{clk::now() - t0}.count(), created.size())
	return {};
}

auto SceneSerializer::serializeEntityToString(const Entity& iEntity) -> std::string {
	const core::Serializer sOut;
	serializeEntity(sOut, iEntity);
	return sOut.getImpl()->emitter.c_str();
}

auto SceneSerializer::deserializeEntityFromString(const shared<Scene>& ioScene, const std::string& iYamlData) -> bool {
	try {
		const core::Serializer sEntity;
		sEntity.getImpl()->node.reset(YAML::Load(iYamlData));
		const auto uuid = sEntity.getImpl()->node["Entity"].as<uint64_t>();
		auto entity = createEntityFromNode(ioScene, sEntity, uuid);
		deserializeEntityComponents(entity, sEntity);
		linkIntoHierarchy(*ioScene, entity);
	} catch (...) {
		OWL_CORE_ERROR("Unable to deserialize entity from string.")
		return false;
	}
	return true;
}

auto SceneSerializer::applyEntityFromString(const Entity& iEntity, const std::string& iYamlData) -> bool {
	if (!iEntity) {
		OWL_CORE_ERROR("SceneSerializer: Cannot apply entity data to an invalid entity.")
		return false;
	}
	try {
		const auto node = YAML::Load(iYamlData);
		if (!node["Entity"] || core::UUID{node["Entity"].as<uint64_t>()} != iEntity.getUUID()) {
			OWL_CORE_ERROR("SceneSerializer: Entity data does not describe entity {}.",
						   static_cast<uint64_t>(iEntity.getUUID()))
			return false;
		}
		Entity entity = iEntity;
		if (const auto tag = node["Tag"]; tag && tag["tag"])
			entity.getComponent<component::Tag>().tag = tag["tag"].as<std::string>();
		applyComponent<component::Transform>(entity, node, true);
		applyComponent<component::Visibility>(entity, node, true);
		applyOptionalComponents(entity, node, component::OptionalComponents{});
	} catch (...) {
		OWL_CORE_ERROR("SceneSerializer: Unable to apply entity data from string.")
		return false;
	}
	return true;
}

}// namespace owl::scene
