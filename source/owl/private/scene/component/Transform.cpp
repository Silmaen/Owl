/**
 * @file Transform.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Transform.h"

namespace owl::scene::component {

void Transform::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// Transform
	iOut.getImpl()->emitter << YAML::Key << "translation" << YAML::Value << transform.translation();
	iOut.getImpl()->emitter << YAML::Key << "rotation" << YAML::Value << transform.rotation();
	iOut.getImpl()->emitter << YAML::Key << "scale" << YAML::Value << transform.scale();
	iOut.getImpl()->emitter << YAML::EndMap;// Transform
}

void Transform::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["translation"])
		transform.translation() = iNode.getImpl()->node["translation"].as<math::vec3>();
	if (iNode.getImpl()->node["rotation"])
		transform.rotation() = iNode.getImpl()->node["rotation"].as<math::vec3>();
	if (iNode.getImpl()->node["scale"])
		transform.scale() = iNode.getImpl()->node["scale"].as<math::vec3>();
}

}// namespace owl::scene::component
