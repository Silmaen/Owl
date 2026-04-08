/**
 * @file Hierarchy.cpp
 * @author Silmaen
 * @date 04/08/26
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/Hierarchy.h"

namespace owl::scene::component {

void Hierarchy::serialize(const core::Serializer& iOut) const {
	if (parentId == core::UUID{0} && childrenIds.empty())
		return;// Skip serialization for root entities with no children.
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// Hierarchy
	if (parentId != core::UUID{0})
		iOut.getImpl()->emitter << YAML::Key << "parentId" << YAML::Value << static_cast<uint64_t>(parentId);
	iOut.getImpl()->emitter << YAML::EndMap;// Hierarchy
}

void Hierarchy::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["parentId"])
		parentId = core::UUID{iNode.getImpl()->node["parentId"].as<uint64_t>()};
	// childrenIds are rebuilt from parentId relationships after all entities are loaded.
}

}// namespace owl::scene::component
