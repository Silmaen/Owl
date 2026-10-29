/**
 * @file ParticleYaml.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "ParticleYaml.h"

#include <magic_enum/magic_enum.hpp>

#include <string>

namespace owl::scene::component::utils {

void emitCurve(YAML::Emitter& ioEmitter, const char* iKey, const math::Curve& iCurve) {
	if (iCurve.empty())
		return;
	ioEmitter << YAML::Key << iKey << YAML::Value << YAML::BeginMap;
	ioEmitter << YAML::Key << "interpolation" << YAML::Value
			  << std::string{magic_enum::enum_name(iCurve.getInterpolation())};
	ioEmitter << YAML::Key << "keys" << YAML::Value << YAML::BeginSeq;
	for (const auto& key: iCurve.keys())
		ioEmitter << YAML::Flow << YAML::BeginSeq << key.time << key.value << YAML::EndSeq;
	ioEmitter << YAML::EndSeq << YAML::EndMap;
}

void readCurve(const core::YamlNode& iNode, math::Curve& oCurve) {
	oCurve.clear();
	if (!iNode || !iNode.isMap())
		return;
	if (const auto interpolation = iNode["interpolation"]) {
		if (const auto cast = magic_enum::enum_cast<math::CurveInterpolation>(interpolation.as<std::string>());
			cast.has_value())
			oCurve.setInterpolation(cast.value());
	}
	if (const auto keys = iNode["keys"]; keys && keys.isSequence()) {
		for (const auto entry: keys) {
			if (entry.isSequence() && entry.size() >= 2)
				oCurve.addKey({entry[0].as<float>(), entry[1].as<float>()});
		}
	}
}

void emitGradient(YAML::Emitter& ioEmitter, const char* iKey, const renderer::ColorGradient& iGradient) {
	if (iGradient.keys.empty())
		return;
	ioEmitter << YAML::Key << iKey << YAML::Value << YAML::BeginSeq;
	for (const auto& key: iGradient.keys)
		ioEmitter << YAML::Flow << YAML::BeginSeq << key.time << key.color.x() << key.color.y() << key.color.z()
				  << key.color.w() << YAML::EndSeq;
	ioEmitter << YAML::EndSeq;
}

void readGradient(const core::YamlNode& iNode, renderer::ColorGradient& oGradient) {
	oGradient.keys.clear();
	if (!iNode || !iNode.isSequence())
		return;
	for (const auto entry: iNode) {
		if (entry.isSequence() && entry.size() >= 5)
			oGradient.addKey({.time = entry[0].as<float>(),
							  .color = {entry[1].as<float>(), entry[2].as<float>(), entry[3].as<float>(),
										entry[4].as<float>()}});
	}
}

}// namespace owl::scene::component::utils
