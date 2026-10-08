/**
 * @file EntityLinkMigration.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EntityLinkMigration.h"

#include "core/SerializerImpl.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace owl::scene {

auto bindEntityLinksByName(const core::Serializer& ioDocument) -> bool {
	const auto entities = ioDocument.getImpl()->node["Entities"];
	if (!entities || !entities.IsSequence())
		return true;
	std::unordered_map<std::string, uint64_t> byTag;
	for (const auto& entity: entities) {
		if (!entity.IsMap() || !entity["Entity"] || !entity["Tag"] || !entity["Tag"]["tag"])
			continue;
		byTag.emplace(entity["Tag"]["tag"].as<std::string>(), entity["Entity"].as<uint64_t>());
	}
	for (auto entity: entities) {
		if (!entity.IsMap())
			continue;
		auto link = entity["EntityLink"];
		if (!link || !link.IsMap() || link["linkedEntityId"] || !link["linkedEntityName"])
			continue;
		if (const auto it = byTag.find(link["linkedEntityName"].as<std::string>()); it != byTag.end())
			link["linkedEntityId"] = it->second;
	}
	return true;
}

}// namespace owl::scene
