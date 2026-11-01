/**
 * @file ShipController.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/ShipController.h"

#include <algorithm>

namespace owl::scene::component {

void ShipController::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// ShipController
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "thrust" << YAML::Value << thrust;
	out << YAML::Key << "strafeThrust" << YAML::Value << strafeThrust;
	out << YAML::Key << "maxSpeed" << YAML::Value << maxSpeed;
	out << YAML::Key << "turnRate" << YAML::Value << turnRate;
	out << YAML::Key << "rollRate" << YAML::Value << rollRate;
	out << YAML::Key << "mouseSensitivity" << YAML::Value << mouseSensitivity;
	out << YAML::Key << "angularResponse" << YAML::Value << angularResponse;
	out << YAML::Key << "assistDamping" << YAML::Value << assistDamping;
	out << YAML::Key << "flightAssist" << YAML::Value << flightAssist;
	out << YAML::Key << "captureCursor" << YAML::Value << captureCursor;
	out << YAML::EndMap;// ShipController
}

void ShipController::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["thrust"])
		thrust = std::clamp(node["thrust"].as<float>(), 0.f, 10000.f);
	if (node["strafeThrust"])
		strafeThrust = std::clamp(node["strafeThrust"].as<float>(), 0.f, 10000.f);
	if (node["maxSpeed"])
		maxSpeed = std::clamp(node["maxSpeed"].as<float>(), 0.f, 1.e6f);
	if (node["turnRate"])
		turnRate = std::clamp(node["turnRate"].as<float>(), 0.f, 20.f);
	if (node["rollRate"])
		rollRate = std::clamp(node["rollRate"].as<float>(), 0.f, 20.f);
	if (node["mouseSensitivity"])
		mouseSensitivity = std::clamp(node["mouseSensitivity"].as<float>(), 0.0001f, 0.05f);
	if (node["angularResponse"])
		angularResponse = std::clamp(node["angularResponse"].as<float>(), 0.1f, 100.f);
	if (node["assistDamping"])
		assistDamping = std::clamp(node["assistDamping"].as<float>(), 0.f, 100.f);
	if (node["flightAssist"])
		flightAssist = node["flightAssist"].as<bool>();
	if (node["captureCursor"])
		captureCursor = node["captureCursor"].as<bool>();
	m_velocity = {0.f, 0.f, 0.f};
	m_angularVelocity = {0.f, 0.f, 0.f};
	m_throttle = 0.f;
	m_mouseValid = false;
}

}// namespace owl::scene::component
