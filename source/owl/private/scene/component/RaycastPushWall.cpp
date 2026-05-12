/**
 * @file RaycastPushWall.cpp
 * @author Silmaen
 * @date 11/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/RaycastPushWall.h"

namespace owl::scene::component {

void RaycastPushWall::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	if (!tilesetPath.empty())
		iOut.getImpl()->emitter << YAML::Key << "tilesetPath" << YAML::Value << tilesetPath;
	iOut.getImpl()->emitter << YAML::Key << "tileIndex" << YAML::Value << tileIndex;
	iOut.getImpl()->emitter << YAML::Key << "slideDirection" << YAML::Value << YAML::Flow << YAML::BeginSeq
							<< slideDirection.x() << slideDirection.y() << YAML::EndSeq;
	iOut.getImpl()->emitter << YAML::Key << "slideDistance" << YAML::Value << slideDistance;
	iOut.getImpl()->emitter << YAML::Key << "slideSpeed" << YAML::Value << slideSpeed;
	iOut.getImpl()->emitter << YAML::Key << "interactionKey" << YAML::Value << static_cast<uint32_t>(interactionKey);
	iOut.getImpl()->emitter << YAML::Key << "interactionRange" << YAML::Value << interactionRange;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void RaycastPushWall::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["tilesetPath"])
		tilesetPath = node["tilesetPath"].as<std::string>();
	if (node["tileIndex"])
		tileIndex = node["tileIndex"].as<uint32_t>();
	if (auto sdNode = node["slideDirection"]; sdNode && sdNode.IsSequence() && sdNode.size() >= 2)
		slideDirection = {sdNode[0].as<float>(), sdNode[1].as<float>()};
	if (node["slideDistance"])
		slideDistance = node["slideDistance"].as<float>();
	if (node["slideSpeed"])
		slideSpeed = node["slideSpeed"].as<float>();
	if (node["interactionKey"])
		interactionKey = static_cast<input::KeyCode>(node["interactionKey"].as<uint32_t>());
	if (node["interactionRange"])
		interactionRange = node["interactionRange"].as<float>();
	// Runtime state is intentionally not persisted: a saved scene always reloads with
	// every pushwall back at its idle position.
	state = State::Idle;
	currentOffset = 0.0f;
	keyHeldLastTick = false;
	tileset.reset();
}

}// namespace owl::scene::component
