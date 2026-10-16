/**
 * @file UiRendererTagMigration.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "UiRendererTagMigration.h"

#include "core/SerializerImpl.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace owl::scene {

namespace {

constexpr auto g_uiLayer = "ui";

auto isHudKey(const std::string_view iKey) -> bool { return iKey == "Canvas" || iKey.starts_with("Ui"); }

auto isUiEntity(const YAML::Node& iEntity) -> bool {
	return std::any_of(iEntity.begin(), iEntity.end(),
					   [](const auto& iItem) -> bool { return isHudKey(iItem.first.template as<std::string>("")); });
}

auto isUiTag(const YAML::Node& iTag) -> bool {
	return iTag.IsMap() && iTag["Name"] && iTag["Name"].as<std::string>("") == g_uiLayer;
}

}// namespace

auto dropUiRendererTag(const core::Serializer& ioDocument) -> bool {
	const auto entities = ioDocument.getImpl()->document["Entities"];
	if (!entities || !entities.IsSequence())
		return true;
	for (auto entity: entities) {
		if (entity.IsMap() && isUiTag(entity["RendererTag"]) && isUiEntity(entity))
			entity.remove("RendererTag");
	}
	return true;
}

auto hasUiRendererTag(const core::YamlNode& iEntity) -> bool {
	if (!iEntity.isMap() || !iEntity["RendererTag"].isMap() || iEntity["RendererTag"]["Name"].getScalar() != g_uiLayer)
		return false;
	return std::ranges::any_of(iEntity, [](const core::YamlNode& iField) -> bool { return isHudKey(iField.getKey()); });
}

}// namespace owl::scene
