/**
 * @file PhysicJoint.cpp
 * @author Silmaen
 * @date 05/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/PhysicJoint.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace owl::scene::component {

void PhysicJoint::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// PhysicJoint
	out << YAML::Key << "type" << YAML::Value << std::string(magic_enum::enum_name(joint.type));
	out << YAML::Key << "connected" << YAML::Value << static_cast<uint64_t>(joint.connected);
	out << YAML::Key << "anchor" << YAML::Value << joint.anchor;
	out << YAML::Key << "connectedAnchor" << YAML::Value << joint.connectedAnchor;
	out << YAML::Key << "axis" << YAML::Value << joint.axis;
	out << YAML::Key << "limited" << YAML::Value << joint.limited;
	out << YAML::Key << "lowerLimit" << YAML::Value << joint.lowerLimit;
	out << YAML::Key << "upperLimit" << YAML::Value << joint.upperLimit;
	out << YAML::Key << "motor" << YAML::Value << joint.motor;
	out << YAML::Key << "motorSpeed" << YAML::Value << joint.motorSpeed;
	out << YAML::Key << "motorForce" << YAML::Value << joint.motorForce;
	out << YAML::Key << "springFrequency" << YAML::Value << joint.springFrequency;
	out << YAML::Key << "springDamping" << YAML::Value << joint.springDamping;
	if (joint.breakForce > 0.f)
		out << YAML::Key << "breakForce" << YAML::Value << joint.breakForce;
	if (joint.breakTorque > 0.f)
		out << YAML::Key << "breakTorque" << YAML::Value << joint.breakTorque;
	out << YAML::EndMap;// PhysicJoint
}

void PhysicJoint::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	joint = {};
	if (node["type"])
		joint.type = magic_enum::enum_cast<SceneJoint::Type>(node["type"].as<std::string>()).value_or(joint.type);
	if (node["connected"])
		joint.connected = core::UUID{node["connected"].as<uint64_t>()};
	if (node["anchor"])
		joint.anchor = node["anchor"].as<math::vec3f>();
	if (node["connectedAnchor"])
		joint.connectedAnchor = node["connectedAnchor"].as<math::vec3f>();
	if (node["axis"])
		joint.axis = node["axis"].as<math::vec3f>();
	if (node["limited"])
		joint.limited = node["limited"].as<bool>();
	if (node["lowerLimit"])
		joint.lowerLimit = node["lowerLimit"].as<float>();
	if (node["upperLimit"])
		joint.upperLimit = node["upperLimit"].as<float>();
	if (node["motor"])
		joint.motor = node["motor"].as<bool>();
	if (node["motorSpeed"])
		joint.motorSpeed = node["motorSpeed"].as<float>();
	if (node["motorForce"])
		joint.motorForce = node["motorForce"].as<float>();
	if (node["springFrequency"])
		joint.springFrequency = node["springFrequency"].as<float>();
	if (node["springDamping"])
		joint.springDamping = node["springDamping"].as<float>();
	if (node["breakForce"])
		joint.breakForce = std::max(node["breakForce"].as<float>(), 0.f);
	if (node["breakTorque"])
		joint.breakTorque = std::max(node["breakTorque"].as<float>(), 0.f);
}

}// namespace owl::scene::component
