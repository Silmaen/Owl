/**
 * @file UiLayout.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiLayout.h"

#include "Vec2Yaml.h"

#include <magic_enum/magic_enum.hpp>

#include <cstdint>
#include <string>

namespace owl::scene::component {

void UiLayout::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "arrangement" << YAML::Value << std::string(magic_enum::enum_name(arrangement));
	emitter << YAML::Key << "crossAlign" << YAML::Value << std::string(magic_enum::enum_name(crossAlign));
	emitter << YAML::Key << "spacing" << YAML::Value << spacing;
	emitter << YAML::Key << "padding" << YAML::Value << padding;
	emitter << YAML::Key << "columns" << YAML::Value << columns;
	emitVec2(emitter, "cellSize", cellSize);
	emitter << YAML::EndMap;
}

void UiLayout::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["arrangement"])
		arrangement =
				magic_enum::enum_cast<Arrangement>(node["arrangement"].as<std::string>()).value_or(Arrangement::Column);
	if (node["crossAlign"])
		crossAlign =
				magic_enum::enum_cast<CrossAlign>(node["crossAlign"].as<std::string>()).value_or(CrossAlign::Start);
	if (node["spacing"])
		spacing = node["spacing"].as<float>();
	if (node["padding"])
		padding = node["padding"].as<float>();
	if (node["columns"])
		columns = node["columns"].as<uint32_t>();
	readVec2(node, "cellSize", cellSize);
}

}// namespace owl::scene::component
