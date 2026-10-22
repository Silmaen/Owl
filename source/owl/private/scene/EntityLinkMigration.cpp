/**
 * @file EntityLinkMigration.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EntityLinkMigration.h"

#include "HudLayoutMigration.h"
#include "PhysicalLightMigration.h"
#include "SceneScriptMigration.h"
#include "UiRendererTagMigration.h"
#include "UiTextMigration.h"

#include "core/SerializerImpl.h"

#include <algorithm>
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
	const auto versionNode = iRoot[core::g_FormatVersionKey];
	const auto current = static_cast<int64_t>(iFormat.currentVersion());
	if (versionNode && !versionNode.isScalar())
		return false;
	const int64_t version = versionNode ? versionNode.as<int64_t>(0) : static_cast<int64_t>(core::g_UnversionedFormat);
	if (version == current)
		return true;
	// Only the steps 1 -> 2 to 5 -> 6 exist (6 -> 7 for a scene): a new step must extend this shortcut or drop it.
	const bool isScene = iFormat.name == "Scene";
	if (current != (isScene ? 7 : 6) || version < 1 || version > current)
		return false;
	const auto entities = iRoot["Entities"];
	return std::all_of(entities.begin(), entities.end(), [version, isScene](const auto& iEntity) -> bool {
		if (!iEntity.isMap())
			return true;
		const auto link = iEntity["EntityLink"];
		const bool nameOnlyLink = link.isMap() && !link["linkedEntityId"] && link["linkedEntityName"];
		return (version >= 2 || !nameOnlyLink) && (version >= 3 || !hasLegacyUiText(iEntity)) &&
			   (version >= 4 || !hasLegacyHudLayout(iEntity)) && (version >= 5 || !hasUiRendererTag(iEntity)) &&
			   !hasLegacyLight(iEntity) && (!isScene || version >= 7 || !hasCameraScript(iEntity));
	});
}

}// namespace owl::scene
