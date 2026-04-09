/**
 * @file AnimatedSpriteRenderer.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/AnimatedSpriteRenderer.h"

namespace owl::scene::component {

void AnimatedSpriteRenderer::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// AnimatedSpriteRenderer
	iOut.getImpl()->emitter << YAML::Key << "color" << YAML::Value << color;
	if (texture)
		iOut.getImpl()->emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	iOut.getImpl()->emitter << YAML::Key << "columns" << YAML::Value << columns;
	iOut.getImpl()->emitter << YAML::Key << "rows" << YAML::Value << rows;
	iOut.getImpl()->emitter << YAML::Key << "firstFrame" << YAML::Value << firstFrame;
	iOut.getImpl()->emitter << YAML::Key << "lastFrame" << YAML::Value << lastFrame;
	iOut.getImpl()->emitter << YAML::Key << "frameDuration" << YAML::Value << frameDuration;
	iOut.getImpl()->emitter << YAML::Key << "loop" << YAML::Value << loop;
	iOut.getImpl()->emitter << YAML::EndMap;// AnimatedSpriteRenderer
}

void AnimatedSpriteRenderer::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["color"])
		color = iNode.getImpl()->node["color"].as<math::vec4>();
	if (iNode.getImpl()->node["texture"])
		texture = renderer::Texture2D::createFromSerialized(iNode.getImpl()->node["texture"].as<std::string>());
	if (iNode.getImpl()->node["columns"])
		columns = iNode.getImpl()->node["columns"].as<uint32_t>();
	if (iNode.getImpl()->node["rows"])
		rows = iNode.getImpl()->node["rows"].as<uint32_t>();
	if (iNode.getImpl()->node["firstFrame"])
		firstFrame = iNode.getImpl()->node["firstFrame"].as<uint32_t>();
	if (iNode.getImpl()->node["lastFrame"])
		lastFrame = iNode.getImpl()->node["lastFrame"].as<uint32_t>();
	if (iNode.getImpl()->node["frameDuration"])
		frameDuration = iNode.getImpl()->node["frameDuration"].as<float>();
	if (iNode.getImpl()->node["loop"])
		loop = iNode.getImpl()->node["loop"].as<bool>();
}

}// namespace owl::scene::component
