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
#include "scene/component/components.h"

#include "core/external/yaml.h"

#include <core/Application.h>

namespace YAML {

template<>
struct convert<owl::math::vec3> {
	static auto encode(const owl::math::vec3& iRhs) -> Node {
		Node node;
		node.push_back(iRhs.x());
		node.push_back(iRhs.y());
		node.push_back(iRhs.z());
		node.SetStyle(EmitterStyle::Flow);
		return node;
	}

	static auto decode(const Node& iNode, owl::math::vec3& iRhs) -> bool {
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
	static auto encode(const owl::math::vec4& iRhs) -> Node {
		Node node;
		node.push_back(iRhs.x());
		node.push_back(iRhs.y());
		node.push_back(iRhs.z());
		node.push_back(iRhs.w());
		node.SetStyle(EmitterStyle::Flow);
		return node;
	}

	static auto decode(const Node& iNode, owl::math::vec4& iRhs) -> bool {
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
static auto operator<<(Emitter& ioOut, const owl::math::vec3& iVect) -> Emitter& {
	ioOut << Flow;
	ioOut << BeginSeq << iVect.x() << iVect.y() << iVect.z() << EndSeq;
	return ioOut;
}

static auto operator<<(Emitter& ioOut, const owl::math::vec4& iVect) -> Emitter& {
	ioOut << Flow;
	ioOut << BeginSeq << iVect.x() << iVect.y() << iVect.z() << iVect.w() << EndSeq;
	return ioOut;
}
// NOLINTEND(misc-use-anonymous-namespace)

}// namespace YAML

namespace owl::scene {
SceneSerializer::SceneSerializer(const shared<Scene>& iScene) : mp_scene(iScene) {}

namespace {
void serializeEntity(YAML::Emitter& ioOut, const Entity& iEntity) {
	ioOut << YAML::BeginMap;// Entity
	ioOut << YAML::Key << "Entity" << YAML::Value << iEntity.getUUID();

	if (iEntity.hasComponent<component::Tag>()) {
		ioOut << YAML::Key << "Tag";
		ioOut << YAML::BeginMap;// Tag
		const auto& tag = iEntity.getComponent<component::Tag>().tag;
		ioOut << YAML::Key << "tag" << YAML::Value << tag;
		ioOut << YAML::EndMap;// Tag
	}

	if (iEntity.hasComponent<component::Transform>()) {
		ioOut << YAML::Key << "Transform";
		ioOut << YAML::BeginMap;// Transform
		const auto& [transformation] = iEntity.getComponent<component::Transform>();
		ioOut << YAML::Key << "translation" << YAML::Value << transformation.translation();
		ioOut << YAML::Key << "rotation" << YAML::Value << transformation.rotation();
		ioOut << YAML::Key << "scale" << YAML::Value << transformation.scale();
		ioOut << YAML::EndMap;// Transform
	}

	if (iEntity.hasComponent<component::Camera>()) {
		ioOut << YAML::Key << "Camera";
		ioOut << YAML::BeginMap;// CameraComponent
		const auto& [primary, fixedAspectRatio, camera] = iEntity.getComponent<component::Camera>();
		ioOut << YAML::Key << "camera" << YAML::Value;
		ioOut << YAML::BeginMap;// Camera
		ioOut << YAML::Key << "projectionType" << YAML::Value
			  << std::string(magic_enum::enum_name(camera.getProjectionType()));
		ioOut << YAML::Key << "perspectiveFOV" << YAML::Value << camera.getPerspectiveVerticalFOV();
		ioOut << YAML::Key << "perspectiveNear" << YAML::Value << camera.getPerspectiveNearClip();
		ioOut << YAML::Key << "perspectiveFar" << YAML::Value << camera.getPerspectiveFarClip();
		ioOut << YAML::Key << "orthographicSize" << YAML::Value << camera.getOrthographicSize();
		ioOut << YAML::Key << "orthographicNear" << YAML::Value << camera.getOrthographicNearClip();
		ioOut << YAML::Key << "orthographicFar" << YAML::Value << camera.getOrthographicFarClip();
		ioOut << YAML::EndMap;// Camera
		ioOut << YAML::Key << "primary" << YAML::Value << primary;
		ioOut << YAML::Key << "fixedAspectRatio" << YAML::Value << fixedAspectRatio;
		ioOut << YAML::EndMap;// Camera
	}

	if (iEntity.hasComponent<component::SpriteRenderer>()) {
		ioOut << YAML::Key << "SpriteRenderer";
		ioOut << YAML::BeginMap;// SpriteRenderer
		const auto& [color, texture, tilingFactor] = iEntity.getComponent<component::SpriteRenderer>();
		ioOut << YAML::Key << "color" << YAML::Value << color;
		if (texture) {
			ioOut << YAML::Key << "tilingFactor" << YAML::Value << tilingFactor;
			ioOut << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
		}
		ioOut << YAML::EndMap;// SpriteRenderer
	}
	if (iEntity.hasComponent<component::CircleRenderer>()) {
		ioOut << YAML::Key << "CircleRenderer";
		ioOut << YAML::BeginMap;// CircleRenderer
		const auto& [color, thickness, fade] = iEntity.getComponent<component::CircleRenderer>();
		ioOut << YAML::Key << "color" << YAML::Value << color;
		ioOut << YAML::Key << "thickness" << YAML::Value << thickness;
		ioOut << YAML::Key << "fade" << YAML::Value << fade;
		ioOut << YAML::EndMap;// CircleRenderer
	}
	if (iEntity.hasComponent<component::Text>()) {
		ioOut << YAML::Key << "TextRenderer";
		ioOut << YAML::BeginMap;// TextRenderer
		const auto& [text, font, color, kerning, lineSpacing] = iEntity.getComponent<component::Text>();
		ioOut << YAML::Key << "color" << YAML::Value << color;
		ioOut << YAML::Key << "kerning" << YAML::Value << kerning;
		ioOut << YAML::Key << "lineSpacing" << YAML::Value << lineSpacing;
		ioOut << YAML::Key << "text" << YAML::Value << text;
		if (font && !font->isDefault()) {
			ioOut << YAML::Key << "font" << YAML::Value << font->getName();
		}
		ioOut << YAML::EndMap;// CircleRenderer
	}

	ioOut << YAML::EndMap;// Entity
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
					auto& [transform] = deserializedEntity.getComponent<component::Transform>();
					transform.translation() = transformComponent["translation"].as<math::vec3>();
					transform.rotation() = transformComponent["rotation"].as<math::vec3>();
					transform.scale() = transformComponent["scale"].as<math::vec3>();
				}
				if (auto cameraComponent = entity["Camera"]; cameraComponent) {
					auto& [primary, fixedAspectRatio, camera] = deserializedEntity.addComponent<component::Camera>();
					auto cameraProps = cameraComponent["camera"];
					auto projType = magic_enum::enum_cast<SceneCamera::ProjectionType>(
							cameraProps["projectionType"].as<std::string>());
					if (projType.has_value())
						camera.setProjectionType(projType.value());
					camera.setPerspectiveVerticalFOV(cameraProps["perspectiveFOV"].as<float>());
					camera.setPerspectiveNearClip(cameraProps["perspectiveNear"].as<float>());
					camera.setPerspectiveFarClip(cameraProps["perspectiveFar"].as<float>());

					camera.setOrthographicSize(cameraProps["orthographicSize"].as<float>());
					camera.setOrthographicNearClip(cameraProps["orthographicNear"].as<float>());
					camera.setOrthographicFarClip(cameraProps["orthographicFar"].as<float>());

					primary = cameraComponent["primary"].as<bool>();
					fixedAspectRatio = cameraComponent["fixedAspectRatio"].as<bool>();
				}
				if (auto spriteRendererComponent = entity["SpriteRenderer"]; spriteRendererComponent) {
					auto& [color, texture, tilingFactor] = deserializedEntity.addComponent<component::SpriteRenderer>();
					color = spriteRendererComponent["color"].as<math::vec4>();
					if (spriteRendererComponent["tilingFactor"])
						tilingFactor = spriteRendererComponent["tilingFactor"].as<float>();
					if (spriteRendererComponent["texture"])
						texture = renderer::Texture2D::createFromSerialized(
								spriteRendererComponent["texture"].as<std::string>());
				}
				if (auto circleRendererComponent = entity["CircleRenderer"]; circleRendererComponent) {
					auto& [color, thickness, fade] = deserializedEntity.addComponent<component::CircleRenderer>();
					color = circleRendererComponent["color"].as<math::vec4>();
					thickness = circleRendererComponent["thickness"].as<float>();
					fade = circleRendererComponent["fade"].as<float>();
				}
				if (auto testRendererComponent = entity["TextRenderer"]; testRendererComponent) {
					auto& [text, font, color, kerning, lineSpacing] =
							deserializedEntity.addComponent<component::Text>();
					color = testRendererComponent["color"].as<math::vec4>();
					kerning = testRendererComponent["kerning"].as<float>();
					lineSpacing = testRendererComponent["lineSpacing"].as<float>();
					text = testRendererComponent["text"].as<std::string>();
					if (core::Application::instanced()) {
						auto& lib = core::Application::get().getFontLibrary();
						if (testRendererComponent["font"]) {
							font = lib.getFont(testRendererComponent["font"].as<std::string>());
						} else {
							font = lib.getDefaultFont();
						}
					}
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
