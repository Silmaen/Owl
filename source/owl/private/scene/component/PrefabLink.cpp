/**
 * @file PrefabLink.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/PrefabLink.h"

namespace owl::scene::component {

void PrefabLink::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// PrefabLink
	iOut.getImpl()->emitter << YAML::Key << "prefabAssetPath" << YAML::Value << prefabAssetPath;
	iOut.getImpl()->emitter << YAML::Key << "syncedVersion" << YAML::Value << syncedVersion;
	if (!uuidMapping.empty()) {
		iOut.getImpl()->emitter << YAML::Key << "uuidMapping" << YAML::Value << YAML::BeginSeq;
		for (const auto& [instanceUuid, canonicalUuid]: uuidMapping) {
			iOut.getImpl()->emitter << YAML::BeginMap;
			iOut.getImpl()->emitter << YAML::Key << "inst" << YAML::Value << instanceUuid;
			iOut.getImpl()->emitter << YAML::Key << "canon" << YAML::Value << canonicalUuid;
			iOut.getImpl()->emitter << YAML::EndMap;
		}
		iOut.getImpl()->emitter << YAML::EndSeq;
	}
	if (!overriddenComponents.empty()) {
		iOut.getImpl()->emitter << YAML::Key << "overrides" << YAML::Value << YAML::BeginSeq;
		for (const auto& overrideKey: overriddenComponents) iOut.getImpl()->emitter << overrideKey;
		iOut.getImpl()->emitter << YAML::EndSeq;
	}
	iOut.getImpl()->emitter << YAML::EndMap;// PrefabLink
}

void PrefabLink::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["prefabAssetPath"])
		prefabAssetPath = iNode.getImpl()->node["prefabAssetPath"].as<std::string>();
	if (iNode.getImpl()->node["syncedVersion"])
		syncedVersion = iNode.getImpl()->node["syncedVersion"].as<uint32_t>();
	if (auto mappingNode = iNode.getImpl()->node["uuidMapping"]; mappingNode) {
		uuidMapping.clear();
		for (const auto& entry: mappingNode) {
			uuidMapping.push_back({
					.instanceUuid = entry["inst"].as<uint64_t>(),
					.canonicalUuid = entry["canon"].as<uint64_t>(),
			});
		}
	}
	if (auto overridesNode = iNode.getImpl()->node["overrides"]; overridesNode) {
		overriddenComponents.clear();
		for (const auto& entry: overridesNode) overriddenComponents.push_back(entry.as<std::string>());
	}
}

}// namespace owl::scene::component
