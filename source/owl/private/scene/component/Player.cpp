/**
 * @file Player.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Player.h"

namespace owl::scene::component {

void Player::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "primary" << YAML::Value << primary;
	iOut.getImpl()->emitter << YAML::Key << "linearImpulse" << YAML::Value << player.linearImpulse;
	iOut.getImpl()->emitter << YAML::Key << "jumpImpulse" << YAML::Value << player.jumpImpulse;
	iOut.getImpl()->emitter << YAML::Key << "canJump" << YAML::Value << player.canJump;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void Player::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["primary"])
		primary = iNode.getImpl()->node["primary"].as<bool>();
	if (iNode.getImpl()->node["linearImpulse"]) {
		player.linearImpulse = iNode.getImpl()->node["linearImpulse"].as<float>();
	}
	if (iNode.getImpl()->node["jumpImpulse"]) {
		player.jumpImpulse = iNode.getImpl()->node["jumpImpulse"].as<float>();
	}
	if (iNode.getImpl()->node["canJump"]) {
		player.canJump = iNode.getImpl()->node["canJump"].as<bool>();
	}
}

}// namespace owl::scene::component
