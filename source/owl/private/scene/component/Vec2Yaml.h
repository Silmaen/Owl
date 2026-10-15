/**
 * @file Vec2Yaml.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/SerializerImpl.h"
#include "math/vectors.h"

namespace owl::scene::component {

/**
 * @brief
 *  Write a 2D vector as a flow sequence `[x, y]` under a key.
 * @param[in,out] ioEmitter The YAML emitter, inside a map.
 * @param[in] iKey The key.
 * @param[in] iValue The vector.
 */
inline void emitVec2(YAML::Emitter& ioEmitter, const char* iKey, const math::vec2& iValue) {
	ioEmitter << YAML::Key << iKey << YAML::Value << YAML::Flow << YAML::BeginSeq << iValue.x() << iValue.y()
			  << YAML::EndSeq;
}

/**
 * @brief
 *  Read a 2D vector written by `emitVec2`; the value is left alone when the key is absent or malformed.
 * @param[in] iNode The YAML map.
 * @param[in] iKey The key.
 * @param[out] oValue The vector.
 */
inline void readVec2(const core::YamlNode& iNode, const char* iKey, math::vec2& oValue) {
	if (const auto value = iNode[iKey]; value.isSequence() && value.size() >= 2)
		oValue = {value[0].as<float>(), value[1].as<float>()};
}

}// namespace owl::scene::component
