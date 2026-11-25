/**
 * @file RaycastDoor.cpp
 * @author Silmaen
 * @date 11/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "data/assets/AssetReference.h"
#include "math/YamlSerializers.h"
#include "scene/component/RaycastDoor.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace owl::scene::component {

void RaycastDoor::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	if (!tilesetPath.empty())
		iOut.getImpl()->emitter << YAML::Key << "tilesetPath" << YAML::Value
								<< data::assets::toAssetReference(tilesetPath);
	iOut.getImpl()->emitter << YAML::Key << "faceTileIndex" << YAML::Value << faceTileIndex;
	iOut.getImpl()->emitter << YAML::Key << "lateralTileIndex" << YAML::Value << lateralTileIndex;
	iOut.getImpl()->emitter << YAML::Key << "openingDirection" << YAML::Value
							<< std::string(magic_enum::enum_name(openingDirection));
	iOut.getImpl()->emitter << YAML::Key << "slideSpeed" << YAML::Value << slideSpeed;
	iOut.getImpl()->emitter << YAML::Key << "holdTime" << YAML::Value << holdTime;
	iOut.getImpl()->emitter << YAML::Key << "closeSpeed" << YAML::Value << closeSpeed;
	iOut.getImpl()->emitter << YAML::Key << "interactionAction" << YAML::Value << interactionAction;
	iOut.getImpl()->emitter << YAML::Key << "interactionRange" << YAML::Value << interactionRange;
	if (state != State::Idle) {
		iOut.getImpl()->emitter << YAML::Key << "state" << YAML::Value << std::string(magic_enum::enum_name(state));
		iOut.getImpl()->emitter << YAML::Key << "currentOffset" << YAML::Value << currentOffset;
		iOut.getImpl()->emitter << YAML::Key << "holdTimer" << YAML::Value << holdTimer;
	}
	iOut.getImpl()->emitter << YAML::EndMap;
}

auto RaycastDoor::getSlideAxis() const -> math::vec2 {
	switch (openingDirection) {
		case OpeningDirection::North:
			return {0.f, 1.f};
		case OpeningDirection::South:
			return {0.f, -1.f};
		case OpeningDirection::East:
			return {1.f, 0.f};
		case OpeningDirection::West:
			return {-1.f, 0.f};
	}
	return {0.f, 0.f};
}

void RaycastDoor::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["tilesetPath"])
		tilesetPath = data::assets::toAssetPath(node["tilesetPath"].as<std::string>());
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
	if (node["interactionAction"])
		interactionAction = node["interactionAction"].as<std::string>();
	else if (node["interactionKey"] && node["interactionKey"].as<uint32_t>() == 0)
		interactionAction.clear();
	if (node["interactionRange"])
		interactionRange = node["interactionRange"].as<float>();
	state = State::Idle;
	if (node["state"])
		state = magic_enum::enum_cast<State>(node["state"].as<std::string>()).value_or(State::Idle);
	currentOffset = state != State::Idle && node["currentOffset"]
							? std::clamp(node["currentOffset"].as<float>(), 0.f, 1.f)
							: 0.0f;
	holdTimer = state == State::Open && node["holdTimer"] ? node["holdTimer"].as<float>() : 0.0f;
	tileset.reset();
}

}// namespace owl::scene::component
