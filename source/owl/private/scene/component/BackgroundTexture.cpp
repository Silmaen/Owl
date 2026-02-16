/**
 * @file BackgroundTexture.cpp
 * @author Silmaen
 * @date 02/15/26
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/BackgroundTexture.h"

namespace owl::scene::component {

void BackgroundTexture::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// BackgroundTexture
	iOut.getImpl()->emitter << YAML::Key << "mode" << YAML::Value << static_cast<int>(mode);
	iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << static_cast<int>(type);
	iOut.getImpl()->emitter << YAML::Key << "color" << YAML::Value << color;
	iOut.getImpl()->emitter << YAML::Key << "topColor" << YAML::Value << topColor;
	if (texture) {
		iOut.getImpl()->emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	}
	iOut.getImpl()->emitter << YAML::EndMap;// BackgroundTexture
}

void BackgroundTexture::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["mode"])
		mode = static_cast<Mode>(iNode.getImpl()->node["mode"].as<int>());
	if (iNode.getImpl()->node["type"])
		type = static_cast<Type>(iNode.getImpl()->node["type"].as<int>());
	if (iNode.getImpl()->node["color"])
		color = iNode.getImpl()->node["color"].as<math::vec4>();
	if (iNode.getImpl()->node["topColor"])
		topColor = iNode.getImpl()->node["topColor"].as<math::vec4>();
	if (iNode.getImpl()->node["texture"])
		texture = renderer::Texture2D::createFromSerialized(iNode.getImpl()->node["texture"].as<std::string>());
}

}// namespace owl::scene::component
