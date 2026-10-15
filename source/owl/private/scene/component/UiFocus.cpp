/**
 * @file UiFocus.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiFocus.h"

#include <cstdint>

namespace owl::scene::component {

void UiFocus::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "focusable" << YAML::Value << focusable;
	emitter << YAML::Key << "initial" << YAML::Value << initial;
	emitter << YAML::Key << "up" << YAML::Value << static_cast<uint64_t>(up);
	emitter << YAML::Key << "down" << YAML::Value << static_cast<uint64_t>(down);
	emitter << YAML::Key << "left" << YAML::Value << static_cast<uint64_t>(left);
	emitter << YAML::Key << "right" << YAML::Value << static_cast<uint64_t>(right);
	emitter << YAML::Key << "onFocusCallback" << YAML::Value << onFocusCallback;
	emitter << YAML::Key << "highlightColor" << YAML::Value << highlightColor;
	emitter << YAML::Key << "highlightThickness" << YAML::Value << highlightThickness;
	emitter << YAML::EndMap;
}

void UiFocus::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["focusable"])
		focusable = node["focusable"].as<bool>();
	if (node["initial"])
		initial = node["initial"].as<bool>();
	if (node["up"])
		up = core::UUID{node["up"].as<uint64_t>()};
	if (node["down"])
		down = core::UUID{node["down"].as<uint64_t>()};
	if (node["left"])
		left = core::UUID{node["left"].as<uint64_t>()};
	if (node["right"])
		right = core::UUID{node["right"].as<uint64_t>()};
	if (node["onFocusCallback"])
		onFocusCallback = node["onFocusCallback"].as<std::string>();
	if (node["highlightColor"])
		highlightColor = node["highlightColor"].as<math::vec4>();
	if (node["highlightThickness"])
		highlightThickness = node["highlightThickness"].as<float>();
}

}// namespace owl::scene::component
