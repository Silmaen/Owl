/**
 * @file FirstPersonController.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/FirstPersonController.h"

#include <algorithm>

namespace owl::scene::component {

void FirstPersonController::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// FirstPersonController
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "walkSpeed" << YAML::Value << walkSpeed;
	out << YAML::Key << "runSpeed" << YAML::Value << runSpeed;
	out << YAML::Key << "lookSpeed" << YAML::Value << lookSpeed;
	out << YAML::Key << "mouseSensitivity" << YAML::Value << mouseSensitivity;
	out << YAML::Key << "captureCursor" << YAML::Value << captureCursor;
	out << YAML::Key << "eyeHeight" << YAML::Value << eyeHeight;
	out << YAML::Key << "height" << YAML::Value << height;
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "stepHeight" << YAML::Value << stepHeight;
	out << YAML::Key << "gravity" << YAML::Value << gravity;
	out << YAML::Key << "jumpSpeed" << YAML::Value << jumpSpeed;
	out << YAML::Key << "reach" << YAML::Value << reach;
	out << YAML::EndMap;// FirstPersonController
}

void FirstPersonController::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["walkSpeed"])
		walkSpeed = std::clamp(node["walkSpeed"].as<float>(), 0.f, 50.f);
	if (node["runSpeed"])
		runSpeed = std::clamp(node["runSpeed"].as<float>(), 0.f, 50.f);
	if (node["lookSpeed"])
		lookSpeed = std::clamp(node["lookSpeed"].as<float>(), 0.f, 20.f);
	if (node["mouseSensitivity"])
		mouseSensitivity = std::clamp(node["mouseSensitivity"].as<float>(), 0.0001f, 0.05f);
	if (node["captureCursor"])
		captureCursor = node["captureCursor"].as<bool>();
	if (node["eyeHeight"])
		eyeHeight = std::clamp(node["eyeHeight"].as<float>(), 0.1f, 10.f);
	if (node["height"])
		height = std::clamp(node["height"].as<float>(), 0.2f, 10.f);
	if (node["radius"])
		radius = std::clamp(node["radius"].as<float>(), 0.05f, 5.f);
	if (node["stepHeight"])
		stepHeight = std::clamp(node["stepHeight"].as<float>(), 0.f, 2.f);
	if (node["gravity"])
		gravity = std::clamp(node["gravity"].as<float>(), 0.f, 100.f);
	if (node["jumpSpeed"])
		jumpSpeed = std::clamp(node["jumpSpeed"].as<float>(), 0.f, 50.f);
	if (node["reach"])
		reach = std::clamp(node["reach"].as<float>(), 0.f, 50.f);
	eyeHeight = std::min(eyeHeight, height);
	m_initialized = false;
	m_worldReady = false;
	m_target = core::UUID{0};
}

}// namespace owl::scene::component
