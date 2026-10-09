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
	const auto entities = ioDocument.getImpl()->document["Entities"];
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

auto canReadWithoutMigration(const core::YamlNode& iRoot, const core::DocumentFormat& iFormat) -> bool {
	const auto version = iRoot[core::g_FormatVersionKey];
	const auto current = static_cast<int64_t>(iFormat.currentVersion());
	if (version)
		return version.isScalar() && version.as<int64_t>(0) == current;
	// Only the 1 -> 2 step exists: a new step must extend this shortcut or drop it.
	if (current != 2)
		return false;
	for (const auto entity: iRoot["Entities"]) {
		if (!entity.isMap())
			continue;
		if (const auto link = entity["EntityLink"]; link.isMap() && !link["linkedEntityId"] && link["linkedEntityName"])
			return false;
	}
	return true;
}

}// namespace owl::scene
