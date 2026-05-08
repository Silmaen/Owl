/**
 * @file YamlSerializers.h
 * @author Silmaen
 * @date 10/27/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/yaml.h"
#include "math/vectors.h"

/// @cond
namespace YAML {

template<>
struct convert<owl::math::vec3> {
	/**
	 * @brief
	 *  Encode a 3D vector to a YAML flow-sequence node.
	 * @param[in] iRhs The vector to encode.
	 * @return The resulting YAML node.
	 */
	static auto encode(const owl::math::vec3& iRhs) -> Node {
		Node node;
		node.push_back(iRhs.x());
		node.push_back(iRhs.y());
		node.push_back(iRhs.z());
		node.SetStyle(EmitterStyle::Flow);
		return node;
	}

	/**
	 * @brief
	 *  Decode a YAML node into a 3D vector.
	 * @param[in] iNode YAML node expected to be a 3-element sequence.
	 * @param[out] iRhs Vector to populate on success.
	 * @return True on success; false when the node is not a 3-element sequence.
	 */
	static auto decode(const Node& iNode, owl::math::vec3& iRhs) -> bool {
		if (!iNode.IsSequence() || iNode.size() != 3)
			return false;
		iRhs.x() = iNode[0].as<float>();
		iRhs.y() = iNode[1].as<float>();
		iRhs.z() = iNode[2].as<float>();
		return true;
	}
};

template<>
struct convert<owl::math::vec4> {
	/**
	 * @brief
	 *  Encode a 4D vector to a YAML flow-sequence node.
	 * @param[in] iRhs The vector to encode.
	 * @return The resulting YAML node.
	 */
	static auto encode(const owl::math::vec4& iRhs) -> Node {
		Node node;
		node.push_back(iRhs.x());
		node.push_back(iRhs.y());
		node.push_back(iRhs.z());
		node.push_back(iRhs.w());
		node.SetStyle(EmitterStyle::Flow);
		return node;
	}

	/**
	 * @brief
	 *  Decode a YAML node into a 4D vector.
	 * @param[in] iNode YAML node expected to be a 4-element sequence.
	 * @param[out] iRhs Vector to populate on success.
	 * @return True on success; false when the node is not a 4-element sequence.
	 */
	static auto decode(const Node& iNode, owl::math::vec4& iRhs) -> bool {
		if (!iNode.IsSequence() || iNode.size() != 4)
			return false;
		iRhs.x() = iNode[0].as<float>();
		iRhs.y() = iNode[1].as<float>();
		iRhs.z() = iNode[2].as<float>();
		iRhs.w() = iNode[3].as<float>();
		return true;
	}
};

// NOLINTBEGIN(misc-use-anonymous-namespace)
/**
 * @brief
 *  Stream a 3D vector into a YAML emitter as an inline flow sequence.
 * @param[in,out] ioOut Target emitter.
 * @param[in] iVect Vector to emit.
 * @return The emitter (for chaining).
 */
[[maybe_unused]] static auto operator<<(Emitter& ioOut, const owl::math::vec3& iVect) -> Emitter& {
	ioOut << Flow;
	ioOut << BeginSeq << iVect.x() << iVect.y() << iVect.z() << EndSeq;
	return ioOut;
}

/**
 * @brief
 *  Stream a 4D vector into a YAML emitter as an inline flow sequence.
 * @param[in,out] ioOut Target emitter.
 * @param[in] iVect Vector to emit.
 * @return The emitter (for chaining).
 */
[[maybe_unused]] static auto operator<<(Emitter& ioOut, const owl::math::vec4& iVect) -> Emitter& {
	ioOut << Flow;
	ioOut << BeginSeq << iVect.x() << iVect.y() << iVect.z() << iVect.w() << EndSeq;
	return ioOut;
}
// NOLINTEND(misc-use-anonymous-namespace)

}// namespace YAML
/// @endcond
