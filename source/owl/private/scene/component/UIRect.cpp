/**
 * @file UIRect.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/UIRect.h"

#include <magic_enum/magic_enum.hpp>

namespace owl::scene::component {

void UIRect::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "anchor" << YAML::Value << std::string(magic_enum::enum_name(anchor));
	iOut.getImpl()->emitter << YAML::Key << "pivotX" << YAML::Value << pivot.x();
	iOut.getImpl()->emitter << YAML::Key << "pivotY" << YAML::Value << pivot.y();
	iOut.getImpl()->emitter << YAML::Key << "sizeX" << YAML::Value << size.x();
	iOut.getImpl()->emitter << YAML::Key << "sizeY" << YAML::Value << size.y();
	iOut.getImpl()->emitter << YAML::Key << "offsetX" << YAML::Value << anchorOffset.x();
	iOut.getImpl()->emitter << YAML::Key << "offsetY" << YAML::Value << anchorOffset.y();
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UIRect::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["anchor"])
		anchor = magic_enum::enum_cast<Anchor>(iNode.getImpl()->node["anchor"].as<std::string>())
						 .value_or(Anchor::Center);
	if (iNode.getImpl()->node["pivotX"])
		pivot.x() = iNode.getImpl()->node["pivotX"].as<float>();
	if (iNode.getImpl()->node["pivotY"])
		pivot.y() = iNode.getImpl()->node["pivotY"].as<float>();
	if (iNode.getImpl()->node["sizeX"])
		size.x() = iNode.getImpl()->node["sizeX"].as<float>();
	if (iNode.getImpl()->node["sizeY"])
		size.y() = iNode.getImpl()->node["sizeY"].as<float>();
	if (iNode.getImpl()->node["offsetX"])
		anchorOffset.x() = iNode.getImpl()->node["offsetX"].as<float>();
	if (iNode.getImpl()->node["offsetY"])
		anchorOffset.y() = iNode.getImpl()->node["offsetY"].as<float>();
}

auto UIRect::computePosition(const math::vec2& iParentSize) const -> math::vec2 {
	math::vec2 anchorPos;
	switch (anchor) {
		case Anchor::TopLeft:
			anchorPos = {0.f, iParentSize.y()};
			break;
		case Anchor::TopCenter:
			anchorPos = {iParentSize.x() * 0.5f, iParentSize.y()};
			break;
		case Anchor::TopRight:
			anchorPos = {iParentSize.x(), iParentSize.y()};
			break;
		case Anchor::MiddleLeft:
			anchorPos = {0.f, iParentSize.y() * 0.5f};
			break;
		case Anchor::Center:
			anchorPos = {iParentSize.x() * 0.5f, iParentSize.y() * 0.5f};
			break;
		case Anchor::MiddleRight:
			anchorPos = {iParentSize.x(), iParentSize.y() * 0.5f};
			break;
		case Anchor::BottomLeft:
			anchorPos = {0.f, 0.f};
			break;
		case Anchor::BottomCenter:
			anchorPos = {iParentSize.x() * 0.5f, 0.f};
			break;
		case Anchor::BottomRight:
			anchorPos = {iParentSize.x(), 0.f};
			break;
	}
	// Apply offset and pivot: position is the centre of the rect for Renderer2D.
	const float posX = anchorPos.x() + anchorOffset.x() + size.x() * (0.5f - pivot.x());
	const float posY = anchorPos.y() + anchorOffset.y() + size.y() * (0.5f - pivot.y());
	return {posX, posY};
}

}// namespace owl::scene::component
