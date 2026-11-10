/**
 * @file Visibility.cpp
 * @author Silmaen
 * @date 02/16/26
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/Visibility.h"

namespace owl::scene::component {

void Visibility::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// Visibility
	iOut.getImpl()->emitter << YAML::Key << "gameVisible" << YAML::Value << gameVisible;
	if (gameplay)
		iOut.getImpl()->emitter << YAML::Key << "gameplay" << YAML::Value << gameplay;
	iOut.getImpl()->emitter << YAML::EndMap;// Visibility
}

void Visibility::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["gameVisible"])
		gameVisible = iNode.getImpl()->node["gameVisible"].as<bool>();
	if (iNode.getImpl()->node["gameplay"])
		gameplay = iNode.getImpl()->node["gameplay"].as<bool>();
	// editorVisible is NOT deserialized — always starts as true
}

}// namespace owl::scene::component
