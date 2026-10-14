/**
 * @file UiTextMigration.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "UiTextMigration.h"

#include "core/SerializerImpl.h"

namespace owl::scene {

auto centreLegacyUiText(const core::Serializer& ioDocument) -> bool {
	const auto entities = ioDocument.getImpl()->document["Entities"];
	if (!entities || !entities.IsSequence())
		return true;
	for (auto entity: entities) {
		if (!entity.IsMap() || !entity["UiText"])
			continue;
		auto text = entity["UiText"];
		if (!text.IsMap() || text["horizontalAlignment"])
			continue;
		text.remove("alignment");
		text["horizontalAlignment"] = "Center";
		text["verticalAlignment"] = "Middle";
	}
	return true;
}

auto hasLegacyUiText(const core::YamlNode& iEntity) -> bool {
	if (!iEntity.isMap())
		return false;
	const auto text = iEntity["UiText"];
	return text.isMap() && !text["horizontalAlignment"];
}

}// namespace owl::scene
