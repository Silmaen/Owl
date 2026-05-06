/**
 * @file AnimationClip.cpp
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/external/yaml.h"
#include "scene/AnimationClip.h"

#include <magic_enum/magic_enum.hpp>

#include <fstream>
#include <sstream>

namespace owl::scene {

namespace {

void emitSpeedCurve(YAML::Emitter& ioEmitter, const math::Curve& iCurve) {
	if (iCurve.empty())
		return;
	ioEmitter << YAML::Key << "speedCurve";
	ioEmitter << YAML::Value << YAML::BeginMap;
	ioEmitter << YAML::Key << "interpolation" << YAML::Value
			  << std::string{magic_enum::enum_name(iCurve.getInterpolation())};
	ioEmitter << YAML::Key << "keys" << YAML::Value << YAML::BeginSeq;
	for (const auto& k: iCurve.keys())
		ioEmitter << YAML::Flow << YAML::BeginSeq << k.time << k.value << YAML::EndSeq;
	ioEmitter << YAML::EndSeq;
	ioEmitter << YAML::EndMap;
}

void readSpeedCurve(const YAML::Node& iNode, math::Curve& oCurve) {
	oCurve.clear();
	if (!iNode || !iNode.IsMap())
		return;
	if (const auto interp = iNode["interpolation"]) {
		const auto cast = magic_enum::enum_cast<math::CurveInterpolation>(interp.as<std::string>());
		if (cast.has_value())
			oCurve.setInterpolation(cast.value());
	}
	if (const auto keys = iNode["keys"]; keys && keys.IsSequence()) {
		for (const auto& entry: keys) {
			if (entry.IsSequence() && entry.size() >= 2)
				oCurve.addKey({entry[0].as<float>(), entry[1].as<float>()});
		}
	}
}

}// namespace

auto AnimationClip::serializeToString(const std::string_view iName) const -> std::string {
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "AnimationClip" << YAML::Value << std::string{iName};
	emitter << YAML::Key << "Version" << YAML::Value << 1;
	if (texture)
		emitter << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	emitter << YAML::Key << "columns" << YAML::Value << columns;
	emitter << YAML::Key << "rows" << YAML::Value << rows;
	emitter << YAML::Key << "firstFrame" << YAML::Value << firstFrame;
	emitter << YAML::Key << "lastFrame" << YAML::Value << lastFrame;
	emitter << YAML::Key << "frameDuration" << YAML::Value << frameDuration;
	emitter << YAML::Key << "loop" << YAML::Value << loop;
	emitSpeedCurve(emitter, speedCurve);
	emitter << YAML::EndMap;
	return std::string{emitter.c_str()};
}

auto AnimationClip::deserializeFromString(const std::string_view iYaml) -> bool {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("AnimationClip: failed to parse YAML — {}", e.what())
		return false;
	}
	if (!root || !root.IsMap() || !root["AnimationClip"])
		return false;
	AnimationClip parsed;
	if (root["texture"])
		parsed.texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(root["texture"].as<std::string>());
	if (root["columns"])
		parsed.columns = std::max(1u, root["columns"].as<uint32_t>());
	if (root["rows"])
		parsed.rows = std::max(1u, root["rows"].as<uint32_t>());
	if (root["firstFrame"])
		parsed.firstFrame = root["firstFrame"].as<uint32_t>();
	if (root["lastFrame"])
		parsed.lastFrame = root["lastFrame"].as<uint32_t>();
	if (root["frameDuration"])
		parsed.frameDuration = root["frameDuration"].as<float>();
	if (root["loop"])
		parsed.loop = root["loop"].as<bool>();
	readSpeedCurve(root["speedCurve"], parsed.speedCurve);
	*this = std::move(parsed);
	return true;
}

auto AnimationClip::saveToFile(const std::filesystem::path& iPath, const std::string_view iName) const -> bool {
	std::ofstream out(iPath, std::ios::binary);
	if (!out.is_open()) {
		OWL_CORE_ERROR("AnimationClip: failed to open '{}' for writing", iPath.string())
		return false;
	}
	const auto displayName = iName.empty() ? iPath.stem().string() : std::string{iName};
	out << serializeToString(displayName);
	return out.good();
}

auto AnimationClip::loadFromFile(const std::filesystem::path& iPath) -> bool {
	std::ifstream in(iPath, std::ios::binary);
	if (!in.is_open()) {
		OWL_CORE_ERROR("AnimationClip: failed to open '{}' for reading", iPath.string())
		return false;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	return deserializeFromString(buffer.str());
}

}// namespace owl::scene
