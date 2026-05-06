/**
 * @file RendererTag.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/RendererTag.h"

namespace owl::scene::component {

void RendererTag::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// RendererTag
	iOut.getImpl()->emitter << YAML::Key << "Name" << YAML::Value << rendererName;
	iOut.getImpl()->emitter << YAML::EndMap;// RendererTag
}

void RendererTag::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["Name"])
		rendererName = iNode.getImpl()->node["Name"].as<std::string>();
}

}// namespace owl::scene::component
