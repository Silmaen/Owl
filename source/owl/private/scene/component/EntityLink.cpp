/**
 * @file EntityLink.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/EntityLink.h"

namespace owl::scene::component {

void EntityLink::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// Tag
	iOut.getImpl()->emitter << YAML::Key << "linkedEntityName" << YAML::Value << linkedEntityName;
	iOut.getImpl()->emitter << YAML::EndMap;// Tag
}

void EntityLink::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["linkedEntityName"])
		linkedEntityName = iNode.getImpl()->node["linkedEntityName"].as<std::string>();
}

}// namespace owl::scene::component
