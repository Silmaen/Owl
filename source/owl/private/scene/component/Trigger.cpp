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
	if (trigger.type == SceneTrigger::TriggerType::Timer) {
		iOut.getImpl()->emitter << YAML::Key << "TimerDuration" << YAML::Value << trigger.timerDuration;
		iOut.getImpl()->emitter << YAML::Key << "TimerRepeating" << YAML::Value << trigger.timerRepeating;
	}
	if (trigger.type == SceneTrigger::TriggerType::Interaction) {
		iOut.getImpl()->emitter << YAML::Key << "InteractionRange" << YAML::Value << trigger.interactionRange;
	}
	if (!trigger.callbackName.empty()) {
		iOut.getImpl()->emitter << YAML::Key << "CallbackName" << YAML::Value << trigger.callbackName;
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
	if (iNode.getImpl()->node["TimerDuration"])
		trigger.timerDuration = iNode.getImpl()->node["TimerDuration"].as<float>();
	if (iNode.getImpl()->node["TimerRepeating"])
		trigger.timerRepeating = iNode.getImpl()->node["TimerRepeating"].as<bool>();
	if (iNode.getImpl()->node["InteractionRange"])
		trigger.interactionRange = iNode.getImpl()->node["InteractionRange"].as<float>();
	if (iNode.getImpl()->node["CallbackName"])
		trigger.callbackName = iNode.getImpl()->node["CallbackName"].as<std::string>();
}

}// namespace owl::scene::component
