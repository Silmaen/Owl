/**
 * @file Camera.cpp
 * @author Silmaen
 * @date 1/28/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/component/Camera.h"

namespace owl::scene::component {
void Camera::serialize(YAML::Emitter& ioOut) const {
	ioOut << YAML::Key << key();
	ioOut << YAML::BeginMap;// CameraComponent
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

/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
void Camera::deserialize(const YAML::Node& iNode) {
	auto cameraProps = iNode["camera"];
	const auto projType =
			magic_enum::enum_cast<SceneCamera::ProjectionType>(cameraProps["projectionType"].as<std::string>());
	if (projType.has_value())
		camera.setProjectionType(projType.value());
	camera.setPerspectiveVerticalFOV(cameraProps["perspectiveFOV"].as<float>());
	camera.setPerspectiveNearClip(cameraProps["perspectiveNear"].as<float>());
	camera.setPerspectiveFarClip(cameraProps["perspectiveFar"].as<float>());

	camera.setOrthographicSize(cameraProps["orthographicSize"].as<float>());
	camera.setOrthographicNearClip(cameraProps["orthographicNear"].as<float>());
	camera.setOrthographicFarClip(cameraProps["orthographicFar"].as<float>());

	primary = iNode["primary"].as<bool>();
	fixedAspectRatio = iNode["fixedAspectRatio"].as<bool>();
}
}// namespace owl::scene::component
