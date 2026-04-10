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

namespace owl::scene::component {

void Canvas::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "space" << YAML::Value << std::string(magic_enum::enum_name(space));
	iOut.getImpl()->emitter << YAML::Key << "sortOrder" << YAML::Value << sortOrder;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void Canvas::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["space"])
		space = magic_enum::enum_cast<Space>(iNode.getImpl()->node["space"].as<std::string>())
						.value_or(Space::ScreenOverlay);
	if (iNode.getImpl()->node["sortOrder"])
		sortOrder = iNode.getImpl()->node["sortOrder"].as<int32_t>();
}

}// namespace owl::scene::component
