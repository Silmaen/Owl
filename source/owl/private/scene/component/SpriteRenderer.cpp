/**
 * @file SpriteRenderer.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/SpriteRenderer.h"

namespace owl::scene::component {

void SpriteRenderer::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// SpriteRenderer
	iOut.getImpl()->emitter << YAML::Key << "color" << YAML::Value << color;
	if (texture) {
		iOut.getImpl()->emitter << YAML::Key << "tilingFactor" << YAML::Value << tilingFactor;
		iOut.getImpl()->emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	}
	iOut.getImpl()->emitter << YAML::EndMap;// SpriteRenderer
}

void SpriteRenderer::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["color"])
		color = iNode.getImpl()->node["color"].as<math::vec4>();
	if (iNode.getImpl()->node["tilingFactor"])
		tilingFactor = iNode.getImpl()->node["tilingFactor"].as<float>();
	if (iNode.getImpl()->node["texture"])
		texture = renderer::Texture2D::createFromSerialized(iNode.getImpl()->node["texture"].as<std::string>());
}

}// namespace owl::scene::component
