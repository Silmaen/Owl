/**
 * @file Environment.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "data/assets/AssetReference.h"
#include "scene/component/Environment.h"

#include <string>

namespace owl::scene::component {

void Environment::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Environment
	out << YAML::Key << "hdri" << YAML::Value << data::assets::toAssetReference(hdri.generic_string());
	out << YAML::Key << "intensity" << YAML::Value << intensity;
	out << YAML::Key << "ambient" << YAML::Value << YAML::Flow << YAML::BeginSeq << ambient.x() << ambient.y()
		<< ambient.z() << YAML::EndSeq;
	out << YAML::Key << "rotation" << YAML::Value << rotation;
	out << YAML::Key << "showSky" << YAML::Value << showSky;
	out << YAML::EndMap;// Environment
}

void Environment::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["hdri"])
		hdri = data::assets::toAssetPath(node["hdri"].as<std::string>());
	if (node["intensity"])
		intensity = node["intensity"].as<float>();
	if (const auto color = node["ambient"]; color && color.isSequence() && color.size() >= 3)
		ambient = math::vec3{color[0].as<float>(), color[1].as<float>(), color[2].as<float>()};
	if (node["rotation"])
		rotation = node["rotation"].as<float>();
	if (node["showSky"])
		showSky = node["showSky"].as<bool>();
}

}// namespace owl::scene::component
