/**
 * @file Trigger.cpp
 * @author Silmaen
 * @date 1/28/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/Trigger.h"

namespace owl::scene::component {

void Trigger::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "Type" << YAML::Value << std::string(magic_enum::enum_name(trigger.type));
	if (trigger.type == SceneTrigger::TriggerType::Teleport) {
		iOut.getImpl()->emitter << YAML::Key << "LevelName" << YAML::Value << trigger.levelName;
		iOut.getImpl()->emitter << YAML::Key << "TargetName" << YAML::Value << trigger.targetName;
	}
	iOut.getImpl()->emitter << YAML::EndMap;
}

/**
 * @brief Read this component from YAML node.
 * @param iNode The YAML node to read.
 */
void Trigger::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["Type"]) {
		const auto triggerType =
				magic_enum::enum_cast<SceneTrigger::TriggerType>(iNode.getImpl()->node["Type"].as<std::string>());
		if (triggerType.has_value())
			trigger.type = triggerType.value();
	}
	if (iNode.getImpl()->node["LevelName"])
		trigger.levelName = iNode.getImpl()->node["LevelName"].as<std::string>();
	if (iNode.getImpl()->node["TargetName"])
		trigger.targetName = iNode.getImpl()->node["TargetName"].as<std::string>();
}

}// namespace owl::scene::component
