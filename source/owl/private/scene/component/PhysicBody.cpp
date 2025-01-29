/**
 * @file PhysicBody.cpp
 * @author Silmaen
 * @date 1/28/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
*/
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/PhysicBody.h"

namespace owl::scene::component {

void PhysicBody::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << std::string(magic_enum::enum_name(body.type));
	iOut.getImpl()->emitter << YAML::Key << "fixedRotation" << YAML::Value << body.fixedRotation;
	iOut.getImpl()->emitter << YAML::Key << "colliderSize" << YAML::Value << body.colliderSize;
	iOut.getImpl()->emitter << YAML::Key << "density" << YAML::Value << body.density;
	iOut.getImpl()->emitter << YAML::Key << "restitution" << YAML::Value << body.restitution;
	iOut.getImpl()->emitter << YAML::Key << "friction" << YAML::Value << body.friction;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void PhysicBody::deserialize(const core::Serializer& iNode) {
	body.type = magic_enum::enum_cast<SceneBody::BodyType>(iNode.getImpl()->node["type"].as<std::string>())
						.value_or(SceneBody::BodyType::Static);
	body.fixedRotation = iNode.getImpl()->node["fixedRotation"].as<bool>();
	body.bodyId = 0;
	body.colliderSize = iNode.getImpl()->node["colliderSize"].as<math::vec3f>();
	body.density = iNode.getImpl()->node["density"].as<float>();
	body.restitution = iNode.getImpl()->node["restitution"].as<float>();
	body.friction = iNode.getImpl()->node["friction"].as<float>();
}

}// namespace owl::scene::component
