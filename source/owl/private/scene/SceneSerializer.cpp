/**
 * @file SceneSerializer.cpp
 * @author Silmaen
 * @date 27/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SceneSerializer.h"

#include "EntityLinkMigration.h"
#include "app/Application.h"
#include "core/FormatVersionYaml.h"
#include "core/Serializer.h"
#include "core/SerializerImpl.h"
#include "platform/AtomicFile.h"
#include "renderer/RenderStackYaml.h"
#include "scene/ComponentRegistry.h"
#include "scene/Entity.h"
#include "scene/component/componentsSerialization.h"

#include <cstdint>
#include <exception>
#include <format>
#include <optional>
#include <string>
#include <string_view>
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

auto fixHint(const SceneLoadError iError) -> std::string_view {
	switch (iError) {
		case SceneLoadError::FileUnreadable:
			return "check that the file exists, is readable and that its path is relative to the project folder";
		case SceneLoadError::InvalidYaml:
			return "fix the YAML syntax at the reported line, or restore the file from version control";
		case SceneLoadError::NotAScene:
			return "open the file with its own editor: a scene is an `.owl` file with a root `Scene:` key";
		case SceneLoadError::InvalidEntity:
			return "fix or remove the reported entity in the file, then reload the scene";
		case SceneLoadError::InvalidFormatVersion:
			return "set `FormatVersion` to a positive integer, or remove the key";
		case SceneLoadError::NewerFormatVersion:
			return "open the scene with the Owl version that saved it, or update Owl";
		case SceneLoadError::MigrationFailed:
			return "restore the file from version control and report the issue with the file attached";
	}
	return "check the scene file";
}

SceneSerializer::SceneSerializer(const shared<Scene>& iScene) : mp_scene(iScene) {}

namespace {

// 1 -> 2: entity links reference their target by UUID.
constexpr std::array<core::MigrationStep, 1> g_sceneMigrations{&bindEntityLinksByName};
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
	component::serializeComponents(iEntity, iOut);
	iOut.getImpl()->emitter << YAML::EndMap;// Entity
}

auto createEntityFromNode(const shared<Scene>& ioScene, const core::YamlNode& iNode, const uint64_t iUuid) -> Entity {
	std::string name;
	if (const auto tagComponent = iNode["Tag"]; tagComponent && tagComponent["tag"])
		name = tagComponent["tag"].as<std::string>();
	OWL_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}.", iUuid, name)
	return ioScene->createEntityWithUUID(core::UUID{iUuid}, name);
}

using SeenUuids = std::unordered_set<uint64_t>;

auto isValidEntityNode(const core::YamlNode& iNode) -> bool { return iNode.isMap() && iNode["Entity"].isScalar(); }

auto describeEntityNode(const core::YamlNode& iNode, const size_t iIndex) -> std::string {
	std::string label = std::format("#{}", iIndex);
	try {
		if (iNode.isMap() && iNode["Entity"].isScalar())
			label += std::format(" (id {})", iNode["Entity"].getScalar());
		if (iNode.isMap() && iNode["Tag"].isMap() && iNode["Tag"]["tag"].isScalar())
			label += std::format(" '{}'", iNode["Tag"]["tag"].getScalar());
		if (const auto line = iNode.getLine(); line > 0)
			label += std::format(" at line {}", line);
	} catch (...) { label += " (unreadable)"; }
	return label;
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

void serializePhysicsSettings(YAML::Emitter& ioEmitter, const PhysicsSettings& iSettings) {
	if (iSettings == PhysicsSettings{})
		return;
	ioEmitter << YAML::Key << "Physics" << YAML::Value << YAML::BeginMap;
	ioEmitter << YAML::Key << "tickRate" << YAML::Value << iSettings.tickRate;
	ioEmitter << YAML::Key << "maxStepsPerFrame" << YAML::Value << iSettings.maxStepsPerFrame;
	ioEmitter << YAML::Key << "solverSubSteps" << YAML::Value << iSettings.solverSubSteps;
	ioEmitter << YAML::Key << "interpolate" << YAML::Value << iSettings.interpolate;
	ioEmitter << YAML::Key << "workerCount" << YAML::Value << iSettings.workerCount;
	ioEmitter << YAML::EndMap;
}

auto deserializePhysicsSettings(const core::YamlNode& iNode) -> PhysicsSettings {
	PhysicsSettings settings;
	if (!iNode.isMap())
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
	for (const auto [handle, other]: ioScene.registry.view<component::Hierarchy>().each()) {
		if (other.parentId != uuid)
			continue;
		if (const auto childId = ioScene.registry.get<component::ID>(handle).id;
			std::ranges::find(hierarchy.childrenIds, childId) == hierarchy.childrenIds.end())
			hierarchy.childrenIds.push_back(childId);
	}
}

// The current value is written then read back, so that it compares with the incoming one structurally.
auto isUnchanged(const core::Serializer& iCurrent, const std::string_view iKey, const core::YamlNode& iIncoming)
		-> bool {
	const core::YamlDocument current{iCurrent.getImpl()->emitter.c_str()};
	return current.getRoot()[iKey].isSameAs(iIncoming);
}

template<typename Component>
auto isComponentUnchanged(const Component& iComponent, const core::YamlNode& iIncoming) -> bool {
	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	iComponent.serialize(sOut);
	sOut.getImpl()->emitter << YAML::EndMap;
	return isUnchanged(sOut, Component::key(), iIncoming);
}

template<typename Component>
void applyComponent(Entity& ioEntity, const core::YamlNode& iEntityNode, const bool iMandatory,
					const core::Serializer& iScratch) {
	const auto node = iEntityNode[Component::key()];
	const bool present = ioEntity.hasComponent<Component>();
	if (!node) {
		if (iMandatory)
			ioEntity.getComponent<Component>() = Component{};
		else if (present)
			ioEntity.removeComponent<Component>();
		return;
	}
	if (present && isComponentUnchanged(ioEntity.getComponent<Component>(), node))
		return;
	auto& comp = ioEntity.addOrReplaceComponent<Component>();
	iScratch.getImpl()->node = node;
	comp.deserialize(iScratch);
}

auto isDescriptorUnchanged(const ComponentDescriptor& iDesc, const Entity& iEntity, const core::YamlNode& iIncoming)
		-> bool {
	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	iDesc.serialize(iEntity, sOut);
	sOut.getImpl()->emitter << YAML::EndMap;
	return isUnchanged(sOut, iDesc.key, iIncoming);
}

void applyOptionalComponents(Entity& ioEntity, const core::YamlNode& iEntityNode, const core::Serializer& iScratch) {
	for (const auto& desc: ComponentRegistry::getAll()) {
		if (!desc.optional)
			continue;
		const auto node = iEntityNode[desc.key];
		const bool present = desc.has(ioEntity);
		if (!node) {
			if (present)
				desc.remove(ioEntity);
			continue;
		}
		if (present && isDescriptorUnchanged(desc, ioEntity, node))
			continue;
		iScratch.getImpl()->node = node;
		desc.deserialize(ioEntity, iScratch);
	}
}


auto serializeComponentByKey(const Entity& iEntity, const std::string_view iKey) -> std::string {
	const auto& all = ComponentRegistry::getAll();
	const auto desc = std::ranges::find(all, iKey, &ComponentDescriptor::key);
	if (desc == all.end() || !desc->has(iEntity))
		return {};
	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	desc->serialize(iEntity, sOut);
	sOut.getImpl()->emitter << YAML::EndMap;
	return sOut.getImpl()->emitter.c_str();
}

}// namespace

auto SceneSerializer::format() -> const core::DocumentFormat& { return g_sceneFormat; }

auto SceneSerializer::serializeToString() const -> std::string {
	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	sOut.getImpl()->emitter << YAML::Key << "Scene" << YAML::Value << "untitled";
	emitFormatVersion(sOut.getImpl()->emitter, g_sceneFormat);
	mp_scene->assignEntityLinkIds();
	if (const auto& enabled = mp_scene->getEnabledRenderers(); !enabled.isEmpty()) {
		sOut.getImpl()->emitter << YAML::Key << "EnabledRenderers" << YAML::Value << renderer::enabledToYaml(enabled);
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
		OWL_CORE_ERROR("SceneSerializer: Cannot load scene '{}': {}. Fix: {}.", iFilepath.string(),
					   describe(SceneLoadError::FileUnreadable), fixHint(SceneLoadError::FileUnreadable))
		return unexpected{SceneLoadError::FileUnreadable};
	}
	return deserializeFromBuffer(*bytes, iFilepath.string());
}

auto SceneSerializer::deserializeFromBuffer(const std::vector<uint8_t>& iData, const std::string& iSourceName) const
		-> SceneLoadResult {
	const auto parsed = parseBuffer(iData, iSourceName);
	if (!parsed.valid) {
		OWL_CORE_ERROR("SceneSerializer: Cannot load scene '{}': {}. Fix: {}.", iSourceName, parsed.error,
					   fixHint(parsed.failure))
		return unexpected{parsed.failure};
	}
	return applyParsed(parsed);
}

auto SceneSerializer::parseBuffer(const std::vector<uint8_t>& iData, const std::string& iSourceName) -> ParsedScene {
	using clk = std::chrono::steady_clock;
	const auto t0 = clk::now();
	ParsedScene out;
	out.sourceName = iSourceName;
	try {
		const std::string_view text{reinterpret_cast<const char*>(iData.data()), iData.size()};
		auto document = mkShared<const core::YamlDocument>(text, iSourceName);
		if (const auto head = document->getRoot(); !head.isMap() || !head["Scene"].isScalar()) {
			out.error = std::string{describe(SceneLoadError::NotAScene)};
			out.failure = SceneLoadError::NotAScene;
			return out;
		}
		if (!canReadWithoutMigration(document->getRoot(), g_sceneFormat)) {
			std::string migrated{text};
			if (const auto version = upgradeDocumentText(g_sceneFormat, migrated, iSourceName); !version) {
				out.error = std::string{describe(version.error())};
				out.failure = toSceneLoadError(version.error());
				return out;
			}
			document = mkShared<const core::YamlDocument>(migrated, iSourceName);
		}
		out.serializer = mkShared<core::Serializer>();
		out.serializer->getImpl()->source = document;
		out.serializer->getImpl()->node = document->getRoot();
		const auto root = document->getRoot();
		if (const auto entities = root["Entities"]; entities && !entities.isNull() && !entities.isSequence()) {
			out.error = std::string{describe(SceneLoadError::NotAScene)};
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
	const auto& source = iParsed.sourceName.empty() ? iParsed.sceneName : iParsed.sourceName;
	std::vector<Entity> created;
	core::YamlNode current;
	std::optional<size_t> currentIndex;
	const auto fail = [&](const std::string_view iReason) -> SceneLoadResult {
		const auto where = currentIndex ? std::format("entity {}", describeEntityNode(current, *currentIndex))
										: std::string{"the scene header"};
		OWL_CORE_ERROR("SceneSerializer: Cannot load scene '{}': {} {}. Fix: {}.", source, where, iReason,
					   fixHint(SceneLoadError::InvalidEntity))
		rollback(mp_scene, created);
		mp_scene->getEnabledRenderers() = previousRenderers;
		mp_scene->getPhysicsSettings() = previousPhysics;
		return unexpected{SceneLoadError::InvalidEntity};
	};
	try {
		const auto& root = sData.getImpl()->node;
		if (const auto enabled = root["EnabledRenderers"]; enabled)
			mp_scene->getEnabledRenderers() = renderer::enabledFromYaml(enabled);
		mp_scene->getPhysicsSettings() = deserializePhysicsSettings(root["Physics"]);
		SeenUuids seen;
		if (const auto entities = root["Entities"]; entities.isSequence()) {
			created.reserve(entities.size());
			seen.reserve(entities.size());
			const core::Serializer scratch;
			for (const auto entity: entities) {
				currentIndex = created.size();
				current = entity;
				if (!isValidEntityNode(entity))
					return fail("has no `Entity:` id");
				const auto uuid = uniqueUuid(entity["Entity"].as<uint64_t>(), seen, source);
				created.push_back(createEntityFromNode(mp_scene, entity, uuid));
				component::deserializeComponents(created.back(), entity, scratch, true);
			}
		}
	} catch (const std::exception& iEx) { return fail(std::format("is malformed ({})", iEx.what())); } catch (...) {
		return fail("is malformed");
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
		const core::YamlDocument document{iYamlData};
		const auto root = document.getRoot();
		const auto uuid = root["Entity"].as<uint64_t>();
		auto entity = createEntityFromNode(ioScene, root, uuid);
		const core::Serializer scratch;
		component::deserializeComponents(entity, root, scratch, true);
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
		const core::YamlDocument document{iYamlData};
		const auto node = document.getRoot();
		if (!node["Entity"] || core::UUID{node["Entity"].as<uint64_t>()} != iEntity.getUUID()) {
			OWL_CORE_ERROR("SceneSerializer: Entity data does not describe entity {}.",
						   static_cast<uint64_t>(iEntity.getUUID()))
			return false;
		}
		Entity entity = iEntity;
		if (const auto tag = node["Tag"]; tag && tag["tag"])
			entity.getComponent<component::Tag>().tag = tag["tag"].as<std::string>();
		const core::Serializer scratch;
		applyComponent<component::Transform>(entity, node, true, scratch);
		applyComponent<component::Visibility>(entity, node, true, scratch);
		applyOptionalComponents(entity, node, scratch);
	} catch (...) {
		OWL_CORE_ERROR("SceneSerializer: Unable to apply entity data from string.")
		return false;
	}
	return true;
}

auto SceneSerializer::serializeComponentToString(const Entity& iEntity, const std::string_view iComponentKey)
		-> std::string {
	if (!iEntity)
		return {};
	return serializeComponentByKey(iEntity, iComponentKey);
}

auto SceneSerializer::replaceComponentInString(const std::string& iEntityYaml, const std::string_view iComponentKey,
											   const std::string& iComponentYaml) -> std::string {
	try {
		auto entity = YAML::Load(iEntityYaml);
		if (!entity.IsMap()) {
			OWL_CORE_WARN("SceneSerializer: Entity data is not a map, cannot replace component {}.", iComponentKey)
			return {};
		}
		const std::string key{iComponentKey};
		if (iComponentYaml.empty()) {
			entity.remove(key);
		} else {
			const auto component = YAML::Load(iComponentYaml);
			if (!component[key]) {
				OWL_CORE_WARN("SceneSerializer: Component data does not describe component {}.", iComponentKey)
				return {};
			}
			entity[key] = component[key];
		}
		YAML::Emitter out;
		out << entity;
		return out.c_str();
	} catch (const std::exception& e) {
		OWL_CORE_WARN("SceneSerializer: Cannot replace component {}: {}.", iComponentKey, e.what())
		return {};
	}
}

}// namespace owl::scene
