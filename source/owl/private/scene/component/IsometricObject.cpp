/**
 * @file IsometricObject.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "data/assets/AssetReference.h"
#include "math/YamlSerializers.h"
#include "scene/component/IsometricObject.h"

#include <string>

namespace owl::scene::component {

auto IsometricObject::getObject() const -> const renderer::IsometricObjectDef* {
	return atlas ? atlas->find(building) : nullptr;
}

void IsometricObject::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;// IsometricObject
	emitter << YAML::Key << "atlasPath" << YAML::Value << data::assets::toAssetReference(atlasPath.generic_string());
	emitter << YAML::Key << "building" << YAML::Value << building;
	emitter << YAML::Key << "color" << YAML::Value << color;
	emitter << YAML::EndMap;// IsometricObject
}

void IsometricObject::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["atlasPath"])
		atlasPath = data::assets::toAssetPath(node["atlasPath"].as<std::string>());
	if (node["building"])
		building = node["building"].as<std::string>();
	if (node["color"])
		color = node["color"].as<math::vec4>();
	atlas.reset();
}

}// namespace owl::scene::component
