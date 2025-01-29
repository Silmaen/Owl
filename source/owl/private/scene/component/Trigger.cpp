/**
 * @file Trigger.cpp
 * @author Silmaen
 * @date 1/28/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include <scene/component/Trigger.h>

namespace owl::scene::component {

void Trigger::serialize([[maybe_unused]] YAML::Emitter& ioOut) const {
	ioOut << YAML::Key << key();
	ioOut << YAML::BeginMap;
	ioOut << YAML::Key << "Type" << YAML::Value << std::string(magic_enum::enum_name(trigger.type));
	ioOut << YAML::EndMap;
}

/**
 * @brief Read this component from YAML node.
 * @param iNode The YAML node to read.
 */
void Trigger::deserialize([[maybe_unused]] const YAML::Node& iNode) {
	if (iNode["Type"]) {
		const auto triggerType = magic_enum::enum_cast<SceneTrigger::TriggerType>(iNode["Type"].as<std::string>());
		if (triggerType.has_value())
			trigger.type = triggerType.value();
	}
}

}// namespace owl::scene::component
