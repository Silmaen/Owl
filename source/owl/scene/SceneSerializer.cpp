/**
 * @file SceneSerializer.cpp
 * @author Silmaen
 * @date 27/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "SceneSerializer.h"

#include "Entity.h"
#include "scene/component/Camera.h"
#include "scene/component/SpriteRenderer.h"
#include "scene/component/Tag.h"
#include "scene/component/Transform.h"


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
#pragma clang diagnostic ignored "-Wshadow"
#endif
#include <yaml-cpp/yaml.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <magic_enum.hpp>

template<>
struct YAML::convert<glm::vec3> {
	static Node encode(const glm::vec3 &rhs) {
		Node node;
		node.push_back(rhs.x);
		node.push_back(rhs.y);
		node.push_back(rhs.z);
		node.SetStyle(EmitterStyle::Flow);
		return node;
	}
	static bool decode(const Node &node, glm::vec3 &rhs) {
		if (!node.IsSequence() || node.size() != 3)
			return false;
		rhs.x = node[0].as<float>();
		rhs.y = node[1].as<float>();
		rhs.z = node[2].as<float>();
		return true;
	}
};

template<>
struct YAML::convert<glm::vec4> {
	static Node encode(const glm::vec4 &rhs) {
		Node node;
		node.push_back(rhs.x);
		node.push_back(rhs.y);
		node.push_back(rhs.z);
		node.push_back(rhs.w);
		node.SetStyle(EmitterStyle::Flow);
		return node;
	}
	static bool decode(const Node &node, glm::vec4 &rhs) {
		if (!node.IsSequence() || node.size() != 4)
			return false;
		rhs.x = node[0].as<float>();
		rhs.y = node[1].as<float>();
		rhs.z = node[2].as<float>();
		rhs.w = node[3].as<float>();
		return true;
	}
};

static YAML::Emitter &operator<<(YAML::Emitter &out, const glm::vec3 &v) {
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return out;
}

static YAML::Emitter &operator<<(YAML::Emitter &out, const glm::vec4 &v) {
	out << YAML::Flow;
	out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}

namespace owl::scene {

SceneSerializer::SceneSerializer(const shrd<Scene> &scene_) : scene(scene_) {
}

static void serializeEntity(YAML::Emitter &out, Entity entity) {
	out << YAML::BeginMap;                                          // Entity
	out << YAML::Key << "Entity" << YAML::Value << "12837192831273";// TODO: Entity ID goes here

	if (entity.hasComponent<component::Tag>()) {
		out << YAML::Key << "TagComponent";
		out << YAML::BeginMap;// TagComponent
		auto &tag = entity.getComponent<component::Tag>().tag;
		out << YAML::Key << "Tag" << YAML::Value << tag;
		out << YAML::EndMap;// TagComponent
	}

	if (entity.hasComponent<component::Transform>()) {
		out << YAML::Key << "TransformComponent";
		out << YAML::BeginMap;// TransformComponent
		auto &tc = entity.getComponent<component::Transform>();
		out << YAML::Key << "Translation" << YAML::Value << tc.translation;
		out << YAML::Key << "Rotation" << YAML::Value << tc.rotation;
		out << YAML::Key << "Scale" << YAML::Value << tc.scale;
		out << YAML::EndMap;// TransformComponent
	}

	if (entity.hasComponent<component::Camera>()) {
		out << YAML::Key << "CameraComponent";
		out << YAML::BeginMap;// CameraComponent
		auto &cameraComponent = entity.getComponent<component::Camera>();
		auto &camera = cameraComponent.camera;
		out << YAML::Key << "Camera" << YAML::Value;
		out << YAML::BeginMap;// Camera
		out << YAML::Key << "ProjectionType" << YAML::Value << static_cast<int>(camera.getProjectionType());
		out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.getPerspectiveVerticalFOV();
		out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.getPerspectiveNearClip();
		out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.getPerspectiveFarClip();
		out << YAML::Key << "OrthographicSize" << YAML::Value << camera.getOrthographicSize();
		out << YAML::Key << "OrthographicNear" << YAML::Value << camera.getOrthographicNearClip();
		out << YAML::Key << "OrthographicFar" << YAML::Value << camera.getOrthographicFarClip();
		out << YAML::EndMap;// Camera
		out << YAML::Key << "Primary" << YAML::Value << cameraComponent.primary;
		out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.fixedAspectRatio;
		out << YAML::EndMap;// CameraComponent
	}

	if (entity.hasComponent<component::SpriteRenderer>()) {
		out << YAML::Key << "SpriteRendererComponent";
		out << YAML::BeginMap;// SpriteRendererComponent
		auto &spriteRendererComponent = entity.getComponent<component::SpriteRenderer>();
		out << YAML::Key << "Color" << YAML::Value << spriteRendererComponent.color;
		out << YAML::EndMap;// SpriteRendererComponent
	}

	out << YAML::EndMap;// Entity
}

void SceneSerializer::serialize(const std::filesystem::path &filepath) {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Scene" << YAML::Value << "untitled";
	out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
	scene->registry.each([&](auto entityID) {
		Entity entity = {entityID, scene.get()};
		if (!entity)
			return;
		serializeEntity(out, entity);
	});
	out << YAML::EndSeq;
	out << YAML::EndMap;
	std::ofstream fileOut(filepath);
	fileOut << out.c_str();
	fileOut.close();
}

bool SceneSerializer::deserialize(const std::filesystem::path &filepath) {
	YAML::Node data = YAML::LoadFile(filepath.string());
	if (!data["Scene"])
		return false;
	auto sceneName = data["Scene"].as<std::string>();
	OWL_CORE_TRACE("Deserializing scene '{0}'", sceneName)
	auto entities = data["Entities"];
	if (entities) {
		for (auto entity: entities) {
			uint64_t uuid = entity["Entity"].as<uint64_t>();// TODO
			std::string name;
			auto tagComponent = entity["TagComponent"];
			if (tagComponent)
				name = tagComponent["Tag"].as<std::string>();

			OWL_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name)

			Entity deserializedEntity = scene->createEntity(name);

			auto transformComponent = entity["TransformComponent"];
			if (transformComponent) {
				// Entities always have transforms
				auto &tc = deserializedEntity.getComponent<component::Transform>();
				tc.translation = transformComponent["Translation"].as<glm::vec3>();
				tc.rotation = transformComponent["Rotation"].as<glm::vec3>();
				tc.scale = transformComponent["Scale"].as<glm::vec3>();
			}

			auto cameraComponent = entity["CameraComponent"];
			if (cameraComponent) {
				auto &cc = deserializedEntity.addComponent<component::Camera>();
				auto cameraProps = cameraComponent["Camera"];
				cc.camera.setProjectionType(static_cast<SceneCamera::ProjectionType>(cameraProps["ProjectionType"].as<int>()));
				cc.camera.setPerspectiveVerticalFOV(cameraProps["PerspectiveFOV"].as<float>());
				cc.camera.setPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
				cc.camera.setPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());

				cc.camera.setOrthographicSize(cameraProps["OrthographicSize"].as<float>());
				cc.camera.setOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
				cc.camera.setOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());

				cc.primary = cameraComponent["Primary"].as<bool>();
				cc.fixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
			}

			auto spriteRendererComponent = entity["SpriteRendererComponent"];
			if (spriteRendererComponent) {
				auto &src = deserializedEntity.addComponent<component::SpriteRenderer>();
				src.color = spriteRendererComponent["Color"].as<glm::vec4>();
			}
		}
	}

	return true;
}


}// namespace owl::scene
