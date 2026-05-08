/**
 * @file UiButton.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiButton.h"

namespace owl::scene::component {

void UiButton::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "normalColor" << YAML::Value << normalColor;
	iOut.getImpl()->emitter << YAML::Key << "hoverColor" << YAML::Value << hoverColor;
	iOut.getImpl()->emitter << YAML::Key << "pressedColor" << YAML::Value << pressedColor;
	iOut.getImpl()->emitter << YAML::Key << "disabledColor" << YAML::Value << disabledColor;
	iOut.getImpl()->emitter << YAML::Key << "onClickCallback" << YAML::Value << onClickCallback;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UiButton::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["normalColor"])
		normalColor = iNode.getImpl()->node["normalColor"].as<math::vec4>();
	if (iNode.getImpl()->node["hoverColor"])
		hoverColor = iNode.getImpl()->node["hoverColor"].as<math::vec4>();
	if (iNode.getImpl()->node["pressedColor"])
		pressedColor = iNode.getImpl()->node["pressedColor"].as<math::vec4>();
	if (iNode.getImpl()->node["disabledColor"])
		disabledColor = iNode.getImpl()->node["disabledColor"].as<math::vec4>();
	if (iNode.getImpl()->node["onClickCallback"])
		onClickCallback = iNode.getImpl()->node["onClickCallback"].as<std::string>();
	state = State::Normal;
}

}// namespace owl::scene::component
