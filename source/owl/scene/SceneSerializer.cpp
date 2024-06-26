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
#include "scene/component/CircleRenderer.h"
#include "scene/component/SpriteRenderer.h"
#include "scene/component/Tag.h"
#include "scene/component/Transform.h"

#include "core/external/yaml.h"

namespace YAML {

template<>
struct convert<owl::math::vec3> {
	static Node encode(const owl::math::vec3& iRhs) {
		Node node;
		node.push_back(iRhs.x());
		node.push_back(iRhs.y());
		node.push_back(iRhs.z());
		node.SetStyle(EmitterStyle::Flow);
		return node;
	}

	static bool decode(const Node& iNode, owl::math::vec3& iRhs) {
		if (!iNode.IsSequence() || iNode.size() != 3)
			return false;
		iRhs.x() = iNode[0].as<float>();
		iRhs.y() = iNode[1].as<float>();
		iRhs.z() = iNode[2].as<float>();
		return true;
	}
};

template<>
struct convert<owl::math::vec4> {
	static Node encode(const owl::math::vec4& iRhs) {
		Node node;
		node.push_back(iRhs.x());
		node.push_back(iRhs.y());
		node.push_back(iRhs.z());
		node.push_back(iRhs.w());
		node.SetStyle(EmitterStyle::Flow);
		return node;
	}

	static bool decode(const Node& iNode, owl::math::vec4& iRhs) {
		if (!iNode.IsSequence() || iNode.size() != 4)
			return false;
		iRhs.x() = iNode[0].as<float>();
		iRhs.y() = iNode[1].as<float>();
		iRhs.z() = iNode[2].as<float>();
		iRhs.w() = iNode[3].as<float>();
		return true;
	}
};

// NOLINTBEGIN(misc-use-anonymous-namespace)
static Emitter& operator<<(Emitter& out, const owl::math::vec3& v) {
	out << Flow;
	out << BeginSeq << v.x() << v.y() << v.z() << EndSeq;
	return out;
}

static Emitter& operator<<(Emitter& out, const owl::math::vec4& v) {
	out << Flow;
	out << BeginSeq << v.x() << v.y() << v.z() << v.w() << EndSeq;
	return out;
}
// NOLINTEND(misc-use-anonymous-namespace)

}// namespace YAML

namespace owl::scene {
SceneSerializer::SceneSerializer(const shared<Scene>& iScene) : mp_scene(iScene) {}

namespace {
void serializeEntity(YAML::Emitter& out, const Entity& iEntity) {
	out << YAML::BeginMap;// Entity
	out << YAML::Key << "Entity" << YAML::Value << iEntity.getUUID();

	if (iEntity.hasComponent<component::Tag>()) {
		out << YAML::Key << "Tag";
		out << YAML::BeginMap;// Tag
		const auto& tag = iEntity.getComponent<component::Tag>().tag;
		out << YAML::Key << "tag" << YAML::Value << tag;
		out << YAML::EndMap;// Tag
	}

	if (iEntity.hasComponent<component::Transform>()) {
		out << YAML::Key << "Transform";
		out << YAML::BeginMap;// Transform
		auto& tc = iEntity.getComponent<component::Transform>();
		out << YAML::Key << "translation" << YAML::Value << tc.translation;
		out << YAML::Key << "rotation" << YAML::Value << tc.rotation;
		out << YAML::Key << "scale" << YAML::Value << tc.scale;
		out << YAML::EndMap;// Transform
	}

	if (iEntity.hasComponent<component::Camera>()) {
		out << YAML::Key << "Camera";
		out << YAML::BeginMap;// CameraComponent
		const auto& cameraComponent = iEntity.getComponent<component::Camera>();
		const auto& camera = cameraComponent.camera;
		out << YAML::Key << "camera" << YAML::Value;
		out << YAML::BeginMap;// Camera
		out << YAML::Key << "projectionType" << YAML::Value
			<< std::string(magic_enum::enum_name(camera.getProjectionType()));
		out << YAML::Key << "perspectiveFOV" << YAML::Value << camera.getPerspectiveVerticalFOV();
		out << YAML::Key << "perspectiveNear" << YAML::Value << camera.getPerspectiveNearClip();
		out << YAML::Key << "perspectiveFar" << YAML::Value << camera.getPerspectiveFarClip();
		out << YAML::Key << "orthographicSize" << YAML::Value << camera.getOrthographicSize();
		out << YAML::Key << "orthographicNear" << YAML::Value << camera.getOrthographicNearClip();
		out << YAML::Key << "orthographicFar" << YAML::Value << camera.getOrthographicFarClip();
		out << YAML::EndMap;// Camera
		out << YAML::Key << "primary" << YAML::Value << cameraComponent.primary;
		out << YAML::Key << "fixedAspectRatio" << YAML::Value << cameraComponent.fixedAspectRatio;
		out << YAML::EndMap;// Camera
	}

	if (iEntity.hasComponent<component::SpriteRenderer>()) {
		out << YAML::Key << "SpriteRenderer";
		out << YAML::BeginMap;// SpriteRenderer
		auto& spriteRendererComponent = iEntity.getComponent<component::SpriteRenderer>();
		out << YAML::Key << "color" << YAML::Value << spriteRendererComponent.color;
		if (spriteRendererComponent.texture) {
			out << YAML::Key << "tilingFactor" << YAML::Value << spriteRendererComponent.tilingFactor;
			out << YAML::Key << "texture" << YAML::Value << spriteRendererComponent.texture->getSerializeString();
		}
		out << YAML::EndMap;// SpriteRenderer
	}
	if (iEntity.hasComponent<component::CircleRenderer>()) {
		out << YAML::Key << "CircleRenderer";
		out << YAML::BeginMap;// CircleRenderer
		auto& circleRendererComponent = iEntity.getComponent<component::CircleRenderer>();
		out << YAML::Key << "color" << YAML::Value << circleRendererComponent.color;
		out << YAML::Key << "thickness" << YAML::Value << circleRendererComponent.thickness;
		out << YAML::Key << "fade" << YAML::Value << circleRendererComponent.fade;
		out << YAML::EndMap;// CircleRenderer
	}

	out << YAML::EndMap;// Entity
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

bool SceneSerializer::deserialize(const std::filesystem::path& iFilepath) const {
	try {
		YAML::Node data = YAML::LoadFile(iFilepath.string());

		if (!data["Scene"]) {
			OWL_CORE_ERROR("File {} is not a scene.", iFilepath.string())
			return false;
		}
		auto sceneName = data["Scene"].as<std::string>();
		OWL_CORE_TRACE("Deserializing scene '{0}'", sceneName)
		if (auto entities = data["Entities"]; entities) {
			for (auto entity: entities) {
				auto uuid = entity["Entity"].as<uint64_t>();
				std::string name;

				if (auto tagComponent = entity["Tag"]; tagComponent)
					name = tagComponent["tag"].as<std::string>();

				OWL_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name)

				Entity deserializedEntity = mp_scene->createEntityWithUUID(core::UUID{uuid}, name);


				if (auto transformComponent = entity["Transform"]; transformComponent) {
					// Entities always have transforms
					auto& tc = deserializedEntity.getComponent<component::Transform>();
					tc.translation = transformComponent["translation"].as<math::vec3>();
					tc.rotation = transformComponent["rotation"].as<math::vec3>();
					tc.scale = transformComponent["scale"].as<math::vec3>();
				}


				if (auto cameraComponent = entity["Camera"]; cameraComponent) {
					auto& cc = deserializedEntity.addComponent<component::Camera>();
					auto cameraProps = cameraComponent["camera"];
					auto projType = magic_enum::enum_cast<SceneCamera::ProjectionType>(
							cameraProps["projectionType"].as<std::string>());
					if (projType.has_value())
						cc.camera.setProjectionType(projType.value());
					cc.camera.setPerspectiveVerticalFOV(cameraProps["perspectiveFOV"].as<float>());
					cc.camera.setPerspectiveNearClip(cameraProps["perspectiveNear"].as<float>());
					cc.camera.setPerspectiveFarClip(cameraProps["perspectiveFar"].as<float>());

					cc.camera.setOrthographicSize(cameraProps["orthographicSize"].as<float>());
					cc.camera.setOrthographicNearClip(cameraProps["orthographicNear"].as<float>());
					cc.camera.setOrthographicFarClip(cameraProps["orthographicFar"].as<float>());

					cc.primary = cameraComponent["primary"].as<bool>();
					cc.fixedAspectRatio = cameraComponent["fixedAspectRatio"].as<bool>();
				}


				if (auto spriteRendererComponent = entity["SpriteRenderer"]; spriteRendererComponent) {
					auto& src = deserializedEntity.addComponent<component::SpriteRenderer>();
					src.color = spriteRendererComponent["color"].as<math::vec4>();
					if (spriteRendererComponent["tilingFactor"])
						src.tilingFactor = spriteRendererComponent["tilingFactor"].as<float>();
					if (spriteRendererComponent["texture"])
						src.texture = renderer::Texture2D::createFromSerialized(
								spriteRendererComponent["texture"].as<std::string>());
				}


				if (auto circleRendererComponent = entity["CircleRenderer"]; circleRendererComponent) {
					auto& src = deserializedEntity.addComponent<component::CircleRenderer>();
					src.color = circleRendererComponent["color"].as<math::vec4>();
					src.thickness = circleRendererComponent["thickness"].as<float>();
					src.fade = circleRendererComponent["fade"].as<float>();
				}
			}
		}
	} catch (...) {
		OWL_CORE_ERROR("Unable to load scene from file {}", iFilepath.string())
		return false;
	}
	return true;
}

}// namespace owl::scene
