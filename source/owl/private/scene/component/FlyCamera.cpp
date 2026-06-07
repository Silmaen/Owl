/**
 * @file FlyCamera.cpp
 * @author Silmaen
 * @date 06/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/FlyCamera.h"

namespace owl::scene::component {

void FlyCamera::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;// FlyCamera
	emitter << YAML::Key << "MoveSpeed" << YAML::Value << moveSpeed;
	emitter << YAML::Key << "LookSpeed" << YAML::Value << lookSpeed;
	emitter << YAML::EndMap;// FlyCamera
}

void FlyCamera::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (const auto speed = node["MoveSpeed"]; speed)
		moveSpeed = speed.as<float>();
	if (const auto speed = node["LookSpeed"]; speed)
		lookSpeed = speed.as<float>();
}

}// namespace owl::scene::component
