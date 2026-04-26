/**
 * @file UIImage.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UIImage.h"

namespace owl::scene::component {

void UIImage::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "tint" << YAML::Value << tint;
	if (texture)
		iOut.getImpl()->emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UIImage::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["tint"])
		tint = iNode.getImpl()->node["tint"].as<math::vec4>();
	if (iNode.getImpl()->node["texture"])
		texture = renderer::Texture2D::createFromSerializedForDeserialize(
				iNode.getImpl()->node["texture"].as<std::string>());
}

}// namespace owl::scene::component
