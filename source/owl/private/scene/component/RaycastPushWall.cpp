/**
 * @file RaycastPushWall.cpp
 * @author Silmaen
 * @date 11/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "data/assets/AssetReference.h"
#include "math/YamlSerializers.h"
#include "scene/component/RaycastPushWall.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace owl::scene::component {

void RaycastPushWall::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	if (!tilesetPath.empty())
		iOut.getImpl()->emitter << YAML::Key << "tilesetPath" << YAML::Value
								<< data::assets::toAssetReference(tilesetPath);
	iOut.getImpl()->emitter << YAML::Key << "tileIndex" << YAML::Value << tileIndex;
	iOut.getImpl()->emitter << YAML::Key << "slideDirection" << YAML::Value << YAML::Flow << YAML::BeginSeq
							<< slideDirection.x() << slideDirection.y() << YAML::EndSeq;
	iOut.getImpl()->emitter << YAML::Key << "slideDistance" << YAML::Value << slideDistance;
	iOut.getImpl()->emitter << YAML::Key << "slideSpeed" << YAML::Value << slideSpeed;
	iOut.getImpl()->emitter << YAML::Key << "interactionAction" << YAML::Value << interactionAction;
	iOut.getImpl()->emitter << YAML::Key << "interactionRange" << YAML::Value << interactionRange;
	if (state != State::Idle) {
		iOut.getImpl()->emitter << YAML::Key << "state" << YAML::Value << (state == State::Final ? "Final" : "Moving");
		iOut.getImpl()->emitter << YAML::Key << "currentOffset" << YAML::Value << currentOffset;
	}
	iOut.getImpl()->emitter << YAML::EndMap;
}

void RaycastPushWall::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["tilesetPath"])
		tilesetPath = data::assets::toAssetPath(node["tilesetPath"].as<std::string>());
	if (node["tileIndex"])
		tileIndex = node["tileIndex"].as<uint32_t>();
	if (auto sdNode = node["slideDirection"]; sdNode && sdNode.isSequence() && sdNode.size() >= 2)
		slideDirection = {sdNode[0].as<float>(), sdNode[1].as<float>()};
	if (node["slideDistance"])
		slideDistance = node["slideDistance"].as<float>();
	if (node["slideSpeed"])
		slideSpeed = node["slideSpeed"].as<float>();
	if (node["interactionAction"])
		interactionAction = node["interactionAction"].as<std::string>();
	else if (node["interactionKey"] && node["interactionKey"].as<uint32_t>() == 0)
		interactionAction.clear();
	if (node["interactionRange"])
		interactionRange = node["interactionRange"].as<float>();
	state = State::Idle;
	if (const auto saved = node["state"]; saved)
		state = saved.as<std::string>() == "Final" ? State::Final : State::Moving;
	currentOffset = state != State::Idle && node["currentOffset"]
							? std::clamp(node["currentOffset"].as<float>(), 0.f, slideDistance)
							: 0.0f;
	tileset.reset();
}

}// namespace owl::scene::component
