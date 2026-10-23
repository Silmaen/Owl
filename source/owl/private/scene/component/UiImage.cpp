/**
 * @file UiImage.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiImage.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace owl::scene::component {

void UiImage::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "tint" << YAML::Value << tint;
	if (texture)
		iOut.getImpl()->emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	if (!renderTarget.empty())
		iOut.getImpl()->emitter << YAML::Key << "renderTarget" << YAML::Value << renderTarget;
	if (columns > 1 || rows > 1 || frame > 0) {
		iOut.getImpl()->emitter << YAML::Key << "columns" << YAML::Value << columns;
		iOut.getImpl()->emitter << YAML::Key << "rows" << YAML::Value << rows;
		iOut.getImpl()->emitter << YAML::Key << "frame" << YAML::Value << frame;
	}
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UiImage::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["tint"])
		tint = iNode.getImpl()->node["tint"].as<math::vec4>();
	if (iNode.getImpl()->node["texture"])
		texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(
				iNode.getImpl()->node["texture"].as<std::string>());
	if (iNode.getImpl()->node["renderTarget"])
		renderTarget = iNode.getImpl()->node["renderTarget"].as<std::string>();
	if (iNode.getImpl()->node["columns"])
		columns = std::max(1u, iNode.getImpl()->node["columns"].as<uint32_t>());
	if (iNode.getImpl()->node["rows"])
		rows = std::max(1u, iNode.getImpl()->node["rows"].as<uint32_t>());
	if (iNode.getImpl()->node["frame"])
		frame = iNode.getImpl()->node["frame"].as<uint32_t>();
}

auto UiImage::getFrameCoords() const -> std::array<math::vec2, 4> {
	const uint32_t safeColumns = std::max(columns, 1u);
	const uint32_t safeRows = std::max(rows, 1u);
	const uint32_t shown = std::min(frame, safeColumns * safeRows - 1);
	const uint32_t column = shown % safeColumns;
	const uint32_t row = shown / safeColumns;
	const float uMin = static_cast<float>(column) / static_cast<float>(safeColumns);
	const float uMax = static_cast<float>(column + 1) / static_cast<float>(safeColumns);
	const float vMax = 1.f - static_cast<float>(row) / static_cast<float>(safeRows);
	const float vMin = 1.f - static_cast<float>(row + 1) / static_cast<float>(safeRows);
	return {math::vec2{uMin, vMin}, math::vec2{uMax, vMin}, math::vec2{uMax, vMax}, math::vec2{uMin, vMax}};
}

}// namespace owl::scene::component
