/**
 * @file UiList.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiList.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace owl::scene::component {

void UiList::setCount(const uint32_t iCount) {
	itemCount = iCount;
	if (rows.size() > iCount)
		rows.resize(iCount);
	if (selectedIndex >= static_cast<int32_t>(std::min<uint32_t>(iCount, std::numeric_limits<int32_t>::max())))
		selectedIndex = -1;
	hoverIndex = -1;
}

void UiList::invalidateRows() {
	rows.clear();
	iconCache.clear();
}

auto UiList::select(const int64_t iIndex) -> bool {
	const int32_t wanted = iIndex >= 0 && iIndex < static_cast<int64_t>(itemCount) ? static_cast<int32_t>(iIndex) : -1;
	if (wanted == selectedIndex)
		return false;
	selectedIndex = wanted;
	revealSelected = true;
	return true;
}

auto UiList::findRow(const size_t iIndex) const -> const UiListRow* {
	return iIndex < rows.size() && rows[iIndex].bound ? &rows[iIndex] : nullptr;
}

auto UiList::loadIcon(const std::string& iPath) -> shared<renderer::gpu::Texture2D> {
	if (iPath.empty())
		return nullptr;
	if (const auto found = iconCache.find(iPath); found != iconCache.end())
		return found->second;
	auto texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(iPath);
	iconCache.emplace(iPath, texture);
	return texture;
}

void UiList::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "itemCount" << YAML::Value << itemCount;
	emitter << YAML::Key << "rowHeight" << YAML::Value << rowHeight;
	emitter << YAML::Key << "rowSpacing" << YAML::Value << rowSpacing;
	emitter << YAML::Key << "selectedIndex" << YAML::Value << selectedIndex;
	emitter << YAML::Key << "scrollOffset" << YAML::Value << scrollOffset;
	emitter << YAML::Key << "wheelStep" << YAML::Value << wheelStep;
	emitter << YAML::Key << "fontSize" << YAML::Value << fontSize;
	emitter << YAML::Key << "rowColor" << YAML::Value << rowColor;
	emitter << YAML::Key << "rowAltColor" << YAML::Value << rowAltColor;
	emitter << YAML::Key << "hoverColor" << YAML::Value << hoverColor;
	emitter << YAML::Key << "selectedColor" << YAML::Value << selectedColor;
	emitter << YAML::Key << "textColor" << YAML::Value << textColor;
	emitter << YAML::Key << "showScrollBar" << YAML::Value << showScrollBar;
	emitter << YAML::Key << "scrollBarWidth" << YAML::Value << scrollBarWidth;
	emitter << YAML::Key << "trackColor" << YAML::Value << trackColor;
	emitter << YAML::Key << "thumbColor" << YAML::Value << thumbColor;
	emitter << YAML::Key << "onSelectCallback" << YAML::Value << onSelectCallback;
	emitter << YAML::Key << "onBindCallback" << YAML::Value << onBindCallback;
	emitter << YAML::EndMap;
}

namespace {

template<typename Node, typename Value>
void readValue(const Node& iNode, const char* iKey, Value& oValue) {
	if (const auto field = iNode[iKey]; field)
		oValue = field.template as<Value>();
}

}// namespace

void UiList::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	readValue(node, "itemCount", itemCount);
	readValue(node, "rowHeight", rowHeight);
	readValue(node, "rowSpacing", rowSpacing);
	readValue(node, "selectedIndex", selectedIndex);
	readValue(node, "scrollOffset", scrollOffset);
	readValue(node, "wheelStep", wheelStep);
	readValue(node, "fontSize", fontSize);
	readValue(node, "rowColor", rowColor);
	readValue(node, "rowAltColor", rowAltColor);
	readValue(node, "hoverColor", hoverColor);
	readValue(node, "selectedColor", selectedColor);
	readValue(node, "textColor", textColor);
	readValue(node, "showScrollBar", showScrollBar);
	readValue(node, "scrollBarWidth", scrollBarWidth);
	readValue(node, "trackColor", trackColor);
	readValue(node, "thumbColor", thumbColor);
	readValue(node, "onSelectCallback", onSelectCallback);
	readValue(node, "onBindCallback", onBindCallback);
	rows.clear();
	if (selectedIndex >= static_cast<int32_t>(std::min<uint32_t>(itemCount, std::numeric_limits<int32_t>::max())))
		selectedIndex = -1;
}

}// namespace owl::scene::component
