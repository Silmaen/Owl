/**
 * @file SerializerImpl.h
 * @author Silmaen
 * @date 1/29/25
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
#include "core/YamlNode.h"
#include "core/external/yaml.h"
#include "math/YamlSerializers.h"

namespace owl::core {
/**
 * @brief
 *  Implementation structure for the Serializer class.
 *
 * Writing goes through yaml-cpp (`emitter`). Components read their value from `node`, a rapidyaml view (scenes,
 * prefabs, entity snapshots); `document` is the editable yaml-cpp tree of the other formats (format migrations,
 * saves).
 */
struct SerializerImpl {
	/// YAML Emitter.
	YAML::Emitter emitter;
	/// Read-only view of the value being deserialized (owned by `source` or by the caller).
	YamlNode node;
	/// Parsed text that owns `node`, when this serializer owns it.
	shared<const YamlDocument> source;
	/// Editable yaml-cpp tree (format migrations, saves).
	YAML::Node document;
};

}// namespace owl::core
