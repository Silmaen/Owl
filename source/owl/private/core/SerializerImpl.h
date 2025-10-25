/**
 * @file SerializerImpl.h
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
#include "core/external/yaml.h"
#include "math/YamlSerializers.h"

namespace owl::core {

/**
 * @brief
 *  Implementation structure for the Serializer class.
 */
struct SerializerImpl {
	/// YAML Emitter.
	YAML::Emitter emitter;
	/// YAML Node.
	YAML::Node node;
};

}// namespace owl::core
