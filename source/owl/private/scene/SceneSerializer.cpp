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
#include "scene/Entity.h"
#include "scene/component/components.h"

namespace owl::scene {
SceneSerializer::SceneSerializer(const shared<Scene>& iScene) : mp_scene(iScene) {}

namespace {

void serializeEntity(YAML::Emitter& ioOut, const Entity& iEntity) {
	ioOut << YAML::BeginMap;// Entity
	ioOut << YAML::Key << "Entity" << YAML::Value << iEntity.getUUID();
	serializeComponents(iEntity, ioOut, component::serializableComponents{});
	ioOut << YAML::EndMap;// Entity
}

void deserializeEntity(const shared<Scene>& ioScene, const YAML::Node& iNode) {
	auto uuid = iNode["Entity"].as<uint64_t>();
	std::string name;
	if (auto tagComponent = iNode["Tag"]; tagComponent)
		name = tagComponent["tag"].as<std::string>();

	OWL_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name)
	Entity entity = ioScene->createEntityWithUUID(core::UUID{uuid}, name);
	if (auto node = iNode["Transform"]; node) {
		// Entities always have transforms
		auto& comp = entity.getComponent<component::Transform>();
		comp.deserialize(node);
	}
	deserializeComponents(entity, iNode, component::optionalComponents{});
}

}// namespace

void SceneSerializer::serialize(const std::filesystem::path& iFilepath) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Scene" << YAML::Value << "untitled";
	out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
	for (auto&& [e]: mp_scene->registry.storage<entt::entity>().each()) {
		const Entity entity{e, mp_scene.get()};
		if (!entity)
			continue;
		serializeEntity(out, entity);
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	std::ofstream fileOut(iFilepath);
	fileOut << out.c_str();
	fileOut.close();
}

auto SceneSerializer::deserialize(const std::filesystem::path& iFilepath) const -> bool {
	try {
		const YAML::Node data = YAML::LoadFile(iFilepath.string());

		if (!data["Scene"]) {
			OWL_CORE_ERROR("File {} is not a scene.", iFilepath.string())
			return false;
		}
		auto sceneName = data["Scene"].as<std::string>();
		OWL_CORE_TRACE("Deserializing scene '{0}'", sceneName)
		if (auto entities = data["Entities"]; entities) {
			for (auto entity: entities) { deserializeEntity(mp_scene, entity); }
		}
	} catch (...) {
		OWL_CORE_ERROR("Unable to load scene from file {}", iFilepath.string())
		return false;
	}
	return true;
}

}// namespace owl::scene
