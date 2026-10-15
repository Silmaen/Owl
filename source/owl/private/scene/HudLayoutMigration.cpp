/**
 * @file HudLayoutMigration.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "HudLayoutMigration.h"

#include "core/SerializerImpl.h"
#include "scene/component/Canvas.h"
#include "scene/component/UiRect.h"

#include <magic_enum/magic_enum.hpp>

#include <string>

namespace owl::scene {

namespace {

auto readFloat(const YAML::Node& iNode, const char* iKey, const float iDefault) -> float {
	return iNode[iKey] ? iNode[iKey].as<float>() : iDefault;
}

auto makePair(const float iX, const float iY) -> YAML::Node {
	YAML::Node pair(YAML::NodeType::Sequence);
	pair.push_back(iX);
	pair.push_back(iY);
	pair.SetStyle(YAML::EmitterStyle::Flow);
	return pair;
}

void convertRect(YAML::Node& ioRect) {
	using component::UiRect;
	const auto anchor = ioRect["anchor"] ? magic_enum::enum_cast<UiRect::Anchor>(ioRect["anchor"].as<std::string>())
												   .value_or(UiRect::Anchor::Center)
										 : UiRect::Anchor::Center;
	const auto point = UiRect::getAnchorPoint(anchor);
	const float pivotX = readFloat(ioRect, "pivotX", 0.5f);
	const float pivotY = readFloat(ioRect, "pivotY", 0.5f);
	const float sizeX = readFloat(ioRect, "sizeX", 100.f);
	const float sizeY = readFloat(ioRect, "sizeY", 100.f);
	const float offsetX = readFloat(ioRect, "offsetX", 0.f);
	const float offsetY = readFloat(ioRect, "offsetY", 0.f);
	for (const char* key: {"anchor", "pivotX", "pivotY", "sizeX", "sizeY", "offsetX", "offsetY"}) ioRect.remove(key);
	ioRect["anchorMin"] = makePair(point.x(), point.y());
	ioRect["anchorMax"] = makePair(point.x(), point.y());
	ioRect["pivot"] = makePair(pivotX, 1.f - pivotY);
	ioRect["size"] = makePair(sizeX, sizeY);
	ioRect["offset"] = makePair(offsetX, -offsetY);
}

auto isLegacyRect(const YAML::Node& iRect) -> bool { return iRect.IsMap() && !iRect["anchorMin"]; }

auto isLegacyCanvas(const YAML::Node& iCanvas) -> bool { return iCanvas.IsMap() && !iCanvas["scaleMode"]; }

}// namespace

auto convertLegacyHudLayout(const core::Serializer& ioDocument) -> bool {
	const auto entities = ioDocument.getImpl()->document["Entities"];
	if (!entities || !entities.IsSequence())
		return true;
	for (auto entity: entities) {
		if (!entity.IsMap())
			continue;
		if (auto rect = entity["UiRect"]; rect && isLegacyRect(rect))
			convertRect(rect);
		if (auto canvas = entity["Canvas"]; canvas && isLegacyCanvas(canvas)) {
			canvas["referenceResolution"] = makePair(1280.f, 720.f);
			canvas["scaleMode"] = std::string{magic_enum::enum_name(component::Canvas::ScaleMode::ConstantPixels)};
		}
	}
	return true;
}

auto hasLegacyHudLayout(const core::YamlNode& iEntity) -> bool {
	if (!iEntity.isMap())
		return false;
	const auto rect = iEntity["UiRect"];
	const auto canvas = iEntity["Canvas"];
	return (rect.isMap() && !rect["anchorMin"]) || (canvas.isMap() && !canvas["scaleMode"]);
}

}// namespace owl::scene
