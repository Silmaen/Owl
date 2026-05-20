/**
 * @file RaycastDoor.cpp
 * @author Silmaen
 * @date 11/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/RaycastDoor.h"

#include <magic_enum/magic_enum.hpp>

namespace owl::scene::component {

void RaycastDoor::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	if (!tilesetPath.empty())
		iOut.getImpl()->emitter << YAML::Key << "tilesetPath" << YAML::Value << tilesetPath;
	iOut.getImpl()->emitter << YAML::Key << "faceTileIndex" << YAML::Value << faceTileIndex;
	iOut.getImpl()->emitter << YAML::Key << "lateralTileIndex" << YAML::Value << lateralTileIndex;
	iOut.getImpl()->emitter << YAML::Key << "openingDirection" << YAML::Value
							<< std::string(magic_enum::enum_name(openingDirection));
	iOut.getImpl()->emitter << YAML::Key << "slideSpeed" << YAML::Value << slideSpeed;
	iOut.getImpl()->emitter << YAML::Key << "holdTime" << YAML::Value << holdTime;
	iOut.getImpl()->emitter << YAML::Key << "closeSpeed" << YAML::Value << closeSpeed;
	iOut.getImpl()->emitter << YAML::Key << "interactionKey" << YAML::Value << static_cast<uint32_t>(interactionKey);
	iOut.getImpl()->emitter << YAML::Key << "interactionRange" << YAML::Value << interactionRange;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void RaycastDoor::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["tilesetPath"])
		tilesetPath = node["tilesetPath"].as<std::string>();
	if (node["faceTileIndex"])
		faceTileIndex = node["faceTileIndex"].as<uint32_t>();
	if (node["lateralTileIndex"])
		lateralTileIndex = node["lateralTileIndex"].as<uint32_t>();
	if (node["openingDirection"]) {
		if (const auto enumValue = magic_enum::enum_cast<OpeningDirection>(node["openingDirection"].as<std::string>());
			enumValue.has_value())
			openingDirection = enumValue.value();
	}
	if (node["slideSpeed"])
		slideSpeed = node["slideSpeed"].as<float>();
	if (node["holdTime"])
		holdTime = node["holdTime"].as<float>();
	if (node["closeSpeed"])
		closeSpeed = node["closeSpeed"].as<float>();
	if (node["interactionKey"])
		interactionKey = static_cast<input::KeyCode>(node["interactionKey"].as<uint32_t>());
	if (node["interactionRange"])
		interactionRange = node["interactionRange"].as<float>();
	state = State::Idle;
	currentOffset = 0.0f;
	holdTimer = 0.0f;
	keyHeldLastTick = false;
	tileset.reset();
}

}// namespace owl::scene::component
