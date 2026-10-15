/**
 * @file UiToolCursor.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiToolCursor.h"

#include "Vec2Yaml.h"

namespace owl::scene::component {

void UiToolCursor::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	if (texture)
		emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	emitter << YAML::Key << "tint" << YAML::Value << tint;
	emitVec2(emitter, "size", size);
	emitVec2(emitter, "hotspot", hotspot);
	emitter << YAML::Key << "active" << YAML::Value << active;
	emitter << YAML::EndMap;
}

void UiToolCursor::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["texture"])
		texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(node["texture"].as<std::string>());
	if (node["tint"])
		tint = node["tint"].as<math::vec4>();
	readVec2(node, "size", size);
	readVec2(node, "hotspot", hotspot);
	if (node["active"])
		active = node["active"].as<bool>();
}

}// namespace owl::scene::component
