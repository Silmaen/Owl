/**
 * @file Tag.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Tag.h"

namespace owl::scene::component {

void Tag::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// Tag
	iOut.getImpl()->emitter << YAML::Key << "tag" << YAML::Value << tag;
	iOut.getImpl()->emitter << YAML::EndMap;// Tag
}

void Tag::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["tag"])
		tag = iNode.getImpl()->node["tag"].as<std::string>();
}

}// namespace owl::scene::component
