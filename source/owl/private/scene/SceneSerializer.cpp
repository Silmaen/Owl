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
	deserializeComponents(entity, iNode, component::OptionalComponents{});
}

}// namespace

void SceneSerializer::serialize(const std::filesystem::path& iFilepath) const {
	const core::Serializer sOut;
	sOut.getImpl()->emitter << YAML::BeginMap;
	sOut.getImpl()->emitter << YAML::Key << "Scene" << YAML::Value << "untitled";
	sOut.getImpl()->emitter << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
	for (const auto& entity: mp_scene->getAllEntities()) {
		if (!entity)
			continue;
		serializeEntity(sOut, entity);
	}
	sOut.getImpl()->emitter << YAML::EndSeq;
	sOut.getImpl()->emitter << YAML::EndMap;
	std::ofstream fileOut(iFilepath);
	fileOut << sOut.getImpl()->emitter.c_str();
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
		if (auto entities = sData.getImpl()->node["Entities"]; entities) {
			for (auto entity: entities) {
				const core::Serializer sEntity;
				sEntity.getImpl()->node.reset(entity);
				deserializeEntity(mp_scene, sEntity);
			}
		}
	} catch (...) {
		OWL_CORE_ERROR("Unable to load scene from file {}", iFilepath.string())
		return false;
	}
	return true;
}

}// namespace owl::scene
