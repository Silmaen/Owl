/**
 * @file Camera.cpp
 * @author Silmaen
 * @date 1/28/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/Camera.h"

namespace owl::scene::component {

void Camera::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// CameraComponent
	iOut.getImpl()->emitter << YAML::Key << "camera" << YAML::Value;
	iOut.getImpl()->emitter << YAML::BeginMap;// Camera
	iOut.getImpl()->emitter << YAML::Key << "projectionType" << YAML::Value
							<< std::string(magic_enum::enum_name(camera.getProjectionType()));
	iOut.getImpl()->emitter << YAML::Key << "perspectiveFOV" << YAML::Value << camera.getPerspectiveVerticalFov();
	iOut.getImpl()->emitter << YAML::Key << "perspectiveNear" << YAML::Value << camera.getPerspectiveNearClip();
	iOut.getImpl()->emitter << YAML::Key << "perspectiveFar" << YAML::Value << camera.getPerspectiveFarClip();
	iOut.getImpl()->emitter << YAML::Key << "orthographicSize" << YAML::Value << camera.getOrthographicSize();
	iOut.getImpl()->emitter << YAML::Key << "orthographicNear" << YAML::Value << camera.getOrthographicNearClip();
	iOut.getImpl()->emitter << YAML::Key << "orthographicFar" << YAML::Value << camera.getOrthographicFarClip();
	iOut.getImpl()->emitter << YAML::EndMap;// Camera
	iOut.getImpl()->emitter << YAML::Key << "primary" << YAML::Value << primary;
	iOut.getImpl()->emitter << YAML::Key << "fixedAspectRatio" << YAML::Value << fixedAspectRatio;
	iOut.getImpl()->emitter << YAML::EndMap;// Camera
}

void Camera::deserialize(const core::Serializer& iNode) {
	auto cameraProps = iNode.getImpl()->node["camera"];
	const auto projType =
			magic_enum::enum_cast<SceneCamera::ProjectionType>(cameraProps["projectionType"].as<std::string>());
	if (projType.has_value())
		camera.setProjectionType(projType.value());
	camera.setPerspectiveVerticalFov(cameraProps["perspectiveFOV"].as<float>());
	camera.setPerspectiveNearClip(cameraProps["perspectiveNear"].as<float>());
	camera.setPerspectiveFarClip(cameraProps["perspectiveFar"].as<float>());

	camera.setOrthographicSize(cameraProps["orthographicSize"].as<float>());
	camera.setOrthographicNearClip(cameraProps["orthographicNear"].as<float>());
	camera.setOrthographicFarClip(cameraProps["orthographicFar"].as<float>());

	primary = iNode.getImpl()->node["primary"].as<bool>();
	fixedAspectRatio = iNode.getImpl()->node["fixedAspectRatio"].as<bool>();
}

}// namespace owl::scene::component
