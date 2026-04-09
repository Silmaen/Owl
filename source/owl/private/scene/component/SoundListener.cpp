/**
 * @file SoundListener.cpp
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/SoundListener.h"

namespace owl::scene::component {

void SoundListener::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "primary" << YAML::Value << primary;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void SoundListener::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["primary"])
		primary = iNode.getImpl()->node["primary"].as<bool>();
}

}// namespace owl::scene::component
