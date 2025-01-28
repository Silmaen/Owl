/**
 * @file PhysicBody.cpp
 * @author Silmaen
 * @date 1/28/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
*/
#include "owlpch.h"

#include "scene/component/PhysicBody.h"

namespace owl::scene::component {

void PhysicBody::serialize(YAML::Emitter& ioOut) const {
	ioOut << YAML::Key << key();
	ioOut << YAML::BeginMap;
	ioOut << YAML::Key << "type" << YAML::Value << std::string(magic_enum::enum_name(body.type));
	ioOut << YAML::Key << "fixedRotation" << YAML::Value << body.fixedRotation;
	ioOut << YAML::Key << "colliderSize" << YAML::Value << body.colliderSize;
	ioOut << YAML::Key << "density" << YAML::Value << body.density;
	ioOut << YAML::Key << "restitution" << YAML::Value << body.restitution;
	ioOut << YAML::Key << "friction" << YAML::Value << body.friction;
	ioOut << YAML::EndMap;
}

void PhysicBody::deserialize(const YAML::Node& iNode) {
	body.type = magic_enum::enum_cast<SceneBody::BodyType>(iNode["type"].as<std::string>())
						.value_or(SceneBody::BodyType::Static);
	body.fixedRotation = iNode["fixedRotation"].as<bool>();
	body.bodyId = 0;
	body.colliderSize = iNode["colliderSize"].as<math::vec3f>();
	body.density = iNode["density"].as<float>();
	body.restitution = iNode["restitution"].as<float>();
	body.friction = iNode["friction"].as<float>();
}

}// namespace owl::scene::component
