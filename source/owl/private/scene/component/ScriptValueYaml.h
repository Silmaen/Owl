/**
 * @file ScriptValueYaml.h
 * @author Silmaen
 * @date 16/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/YamlNode.h"
#include "core/external/yaml.h"
#include "script/ScriptEngine.h"

namespace owl::scene::component {

/**
 * @brief
 *  Write the value of a script property as YAML (a scalar, or a flow sequence for vectors, colours and lists).
 * @param[in,out] ioEmitter The emitter.
 * @param[in] iProperty The property.
 */
void writeScriptValue(YAML::Emitter& ioEmitter, const script::ScriptProperty& iProperty);

/**
 * @brief
 *  Read a script value of a given type from YAML.
 * @param[in] iNode The node.
 * @param[in] iType The type of the value.
 * @return The value; the default of the type for a vector or colour of the wrong size.
 */
[[nodiscard]] auto readScriptValue(const core::YamlNode& iNode, script::ScriptPropertyType iType)
		-> script::ScriptValue;

}// namespace owl::scene::component
