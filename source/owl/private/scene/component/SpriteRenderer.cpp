/**
 * @file SpriteRenderer.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright (c) 2025 All rights reserved.
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
		if (std::abs(tilingFactor.x() - tilingFactor.y()) < 1e-6f)
			iOut.getImpl()->emitter << YAML::Key << "tilingFactor" << YAML::Value << tilingFactor.x();
		else {
			iOut.getImpl()->emitter << YAML::Key << "tilingFactor" << YAML::Value
									<< YAML::Flow << YAML::BeginSeq << tilingFactor.x() << tilingFactor.y()
									<< YAML::EndSeq;
		}
		iOut.getImpl()->emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	}
	iOut.getImpl()->emitter << YAML::EndMap;// SpriteRenderer
}

void SpriteRenderer::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["color"])
		color = iNode.getImpl()->node["color"].as<math::vec4>();
	if (auto tfNode = iNode.getImpl()->node["tilingFactor"]; tfNode) {
		if (tfNode.IsSequence() && tfNode.size() >= 2)
			tilingFactor = {tfNode[0].as<float>(), tfNode[1].as<float>()};
		else
			tilingFactor = {tfNode.as<float>(), tfNode.as<float>()};
	}
	if (iNode.getImpl()->node["texture"])
		texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(
				iNode.getImpl()->node["texture"].as<std::string>());
}

}// namespace owl::scene::component
