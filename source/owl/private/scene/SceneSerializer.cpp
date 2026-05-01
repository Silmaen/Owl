/**
 * @file SceneSerializer.cpp
 * @author Silmaen
 * @date 27/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SceneSerializer.h"

#include "core/Application.h"
#include "core/Serializer.h"
#include "core/SerializerImpl.h"
#include "scene/Entity.h"
#include "scene/component/componentsSerialization.h"

namespace owl::scene {
SceneSerializer::SceneSerializer(const shared<Scene>& iScene) : mp_scene(iScene) {}

namespace {

void serializeEntity(const core::Serializer& iOut, const Entity& iEntity) {
	iOut.getImpl()->emitter << YAML::BeginMap;// Entity
	iOut.getImpl()->emitter << YAML::Key << "Entity" << YAML::Value << iEntity.getUUID();
	serializeComponents(iEntity, iOut, component::SerializableComponents{});
	iOut.getImpl()->emitter << YAML::EndMap;// Entity
}

void deserializeEntity(const shared<Scene>& ioScene, const core::Serializer& iNode) {
	auto uuid = iNode.getImpl()->node["Entity"].as<uint64_t>();
	std::string name;
	if (auto tagComponent = iNode.getImpl()->node["Tag"]; tagComponent)
		name = tagComponent["tag"].as<std::string>();

	const core::Serializer sNode;
	OWL_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name)
	Entity entity = ioScene->createEntityWithUUID(core::UUID{uuid}, name);
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Transform"]); sNode.getImpl()->node) {
		// Entities always have transforms
		auto& comp = entity.getComponent<component::Transform>();
		comp.deserialize(sNode);
	}
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Visibility"]); sNode.getImpl()->node) {
		// Entities always have visibility
		auto& comp = entity.getComponent<component::Visibility>();
		comp.deserialize(sNode);
	}
	if (sNode.getImpl()->node.reset(iNode.getImpl()->node["Hierarchy"]); sNode.getImpl()->node) {
		// Entities always have hierarchy
		auto& comp = entity.getComponent<component::Hierarchy>();
		comp.deserialize(sNode);
	}
	deserializeComponents(entity, iNode, component::OptionalComponents{});
}

}// namespace

auto SceneSerializer::serializeToString() const -> std::string {
	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	sOut.getImpl()->emitter << YAML::Key << "Scene" << YAML::Value << "untitled";
	if (const auto& enabled = mp_scene->getEnabledRenderers(); !enabled.isEmpty()) {
		sOut.getImpl()->emitter << YAML::Key << "EnabledRenderers" << YAML::Value << enabled.toYaml();
	}
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

void SceneSerializer::serialize(const std::filesystem::path& iFilepath) const {
	std::ofstream fileOut(iFilepath);
	fileOut << serializeToString();
	fileOut.close();
}

auto SceneSerializer::deserialize(const std::filesystem::path& iFilepath) const -> bool {
	try {
		const core::Serializer sData;
		sData.getImpl()->node.reset(YAML::LoadFile(iFilepath.string()));

		if (!sData.getImpl()->node["Scene"]) {
			OWL_CORE_ERROR("File {} is not a scene.", iFilepath.string())
			return false;
		}
		auto sceneName = sData.getImpl()->node["Scene"].as<std::string>();
		OWL_CORE_TRACE("Deserializing scene '{0}'", sceneName)
		if (const auto enabled = sData.getImpl()->node["EnabledRenderers"]; enabled)
			mp_scene->getEnabledRenderers() = renderer::EnabledRenderersConfig::fromYaml(enabled);
		if (auto entities = sData.getImpl()->node["Entities"]; entities) {
			for (auto entity: entities) {
				const core::Serializer sEntity;
				sEntity.getImpl()->node.reset(entity);
				deserializeEntity(mp_scene, sEntity);
			}
		}
		mp_scene->rebuildHierarchyChildren();
	} catch (...) {
		OWL_CORE_ERROR("Unable to load scene from file {}", iFilepath.string())
		return false;
	}
	return true;
}

auto SceneSerializer::deserializeFromBuffer(const std::vector<uint8_t>& iData,
										   const std::string& iSourceName) const -> bool {
	try {
		const std::string yamlStr(iData.begin(), iData.end());
		const core::Serializer sData;
		sData.getImpl()->node.reset(YAML::Load(yamlStr));

		if (!sData.getImpl()->node["Scene"]) {
			OWL_CORE_ERROR("Buffer {} is not a scene.", iSourceName)
			return false;
		}
		auto sceneName = sData.getImpl()->node["Scene"].as<std::string>();
		OWL_CORE_TRACE("Deserializing scene '{0}' from buffer", sceneName)
		if (const auto enabled = sData.getImpl()->node["EnabledRenderers"]; enabled)
			mp_scene->getEnabledRenderers() = renderer::EnabledRenderersConfig::fromYaml(enabled);
		if (auto entities = sData.getImpl()->node["Entities"]; entities) {
			for (auto entity: entities) {
				const core::Serializer sEntity;
				sEntity.getImpl()->node.reset(entity);
				deserializeEntity(mp_scene, sEntity);
			}
		}
		mp_scene->rebuildHierarchyChildren();
	} catch (...) {
		OWL_CORE_ERROR("Unable to load scene from buffer {}", iSourceName)
		return false;
	}
	return true;
}

auto SceneSerializer::serializeEntityToString(const Entity& iEntity) -> std::string {
	const core::Serializer sOut;
	serializeEntity(sOut, iEntity);
	return sOut.getImpl()->emitter.c_str();
}

auto SceneSerializer::deserializeEntityFromString(const shared<Scene>& ioScene,
												  const std::string& iYamlData) -> bool {
	try {
		const core::Serializer sEntity;
		sEntity.getImpl()->node.reset(YAML::Load(iYamlData));
		deserializeEntity(ioScene, sEntity);
		ioScene->rebuildHierarchyChildren();
	} catch (...) {
		OWL_CORE_ERROR("Unable to deserialize entity from string")
		return false;
	}
	return true;
}

}// namespace owl::scene
