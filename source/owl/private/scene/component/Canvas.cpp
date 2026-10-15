/**
 * @file Canvas.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/Canvas.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace owl::scene::component {

auto Canvas::getScale(const math::vec2& iScreenSize) const -> float {
	if (referenceResolution.x() <= 0.f || referenceResolution.y() <= 0.f || iScreenSize.x() <= 0.f ||
		iScreenSize.y() <= 0.f)
		return 1.f;
	const float widthRatio = iScreenSize.x() / referenceResolution.x();
	const float heightRatio = iScreenSize.y() / referenceResolution.y();
	switch (scaleMode) {
		case ScaleMode::ConstantPixels:
			return 1.f;
		case ScaleMode::ScaleWithHeight:
			return heightRatio;
		case ScaleMode::MatchSmallerSide:
			return std::min(widthRatio, heightRatio);
	}
	return 1.f;
}

void Canvas::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "space" << YAML::Value << std::string(magic_enum::enum_name(space));
	iOut.getImpl()->emitter << YAML::Key << "sortOrder" << YAML::Value << sortOrder;
	iOut.getImpl()->emitter << YAML::Key << "referenceResolution" << YAML::Value << YAML::Flow << YAML::BeginSeq
							<< referenceResolution.x() << referenceResolution.y() << YAML::EndSeq;
	iOut.getImpl()->emitter << YAML::Key << "scaleMode" << YAML::Value << std::string(magic_enum::enum_name(scaleMode));
	iOut.getImpl()->emitter << YAML::EndMap;
}

void Canvas::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["space"])
		space = magic_enum::enum_cast<Space>(iNode.getImpl()->node["space"].as<std::string>())
						.value_or(Space::ScreenOverlay);
	if (iNode.getImpl()->node["sortOrder"])
		sortOrder = iNode.getImpl()->node["sortOrder"].as<int32_t>();
	if (const auto resolution = iNode.getImpl()->node["referenceResolution"];
		resolution.isSequence() && resolution.size() >= 2)
		referenceResolution = {resolution[0].as<float>(), resolution[1].as<float>()};
	if (iNode.getImpl()->node["scaleMode"])
		scaleMode = magic_enum::enum_cast<ScaleMode>(iNode.getImpl()->node["scaleMode"].as<std::string>())
							.value_or(ScaleMode::ScaleWithHeight);
}

}// namespace owl::scene::component
