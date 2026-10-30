/**
 * @file StreamingSource.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/component/StreamingSource.h"

#include "core/SerializerImpl.h"

#include <algorithm>
#include <cmath>

namespace owl::scene::component {

void StreamingSource::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key() << YAML::BeginMap;
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "radiusScale" << YAML::Value << radiusScale;
	out << YAML::EndMap;
}

void StreamingSource::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	enabled = node["enabled"] ? node["enabled"].as<bool>() : true;
	radiusScale = node["radiusScale"] ? node["radiusScale"].as<float>() : 1.f;
	radiusScale = std::isfinite(radiusScale) ? std::clamp(radiusScale, minRadiusScale, maxRadiusScale) : 1.f;
}

}// namespace owl::scene::component
