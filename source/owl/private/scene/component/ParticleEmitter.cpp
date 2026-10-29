/**
 * @file ParticleEmitter.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/component/ParticleEmitter.h"

#include "ParticleYaml.h"
#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace owl::scene::component {

namespace {

template<typename Enum>
void readEnum(const core::YamlNode& iNode, Enum& oValue) {
	if (!iNode)
		return;
	if (const auto parsed = magic_enum::enum_cast<Enum>(iNode.as<std::string>()); parsed.has_value())
		oValue = parsed.value();
}

template<typename T>
void readValue(const core::YamlNode& iNode, T& oValue) {
	if (iNode)
		oValue = iNode.as<T>();
}

void readEmission(const core::YamlNode& iNode, renderer::ParticleEmitterSettings& ioSettings) {
	readValue(iNode["maxParticles"], ioSettings.maxParticles);
	ioSettings.maxParticles = std::clamp(ioSettings.maxParticles, 1u, renderer::g_maxParticles);
	readValue(iNode["rate"], ioSettings.rate);
	readValue(iNode["burst"], ioSettings.burst);
	readValue(iNode["duration"], ioSettings.duration);
	readValue(iNode["loop"], ioSettings.loop);
	readValue(iNode["lifetimeMin"], ioSettings.lifetimeMin);
	readValue(iNode["lifetimeMax"], ioSettings.lifetimeMax);
	readEnum(iNode["shape"], ioSettings.shape);
	readValue(iNode["shapeSize"], ioSettings.shapeSize);
	readValue(iNode["coneAngle"], ioSettings.coneAngle);
	readValue(iNode["speedMin"], ioSettings.speedMin);
	readValue(iNode["speedMax"], ioSettings.speedMax);
	readValue(iNode["velocity"], ioSettings.velocity);
	readValue(iNode["gravity"], ioSettings.gravity);
	readValue(iNode["drag"], ioSettings.drag);
	readValue(iNode["seed"], ioSettings.seed);
}

void readLook(const core::YamlNode& iNode, renderer::ParticleEmitterSettings& ioSettings) {
	readValue(iNode["sizeMin"], ioSettings.sizeMin);
	readValue(iNode["sizeMax"], ioSettings.sizeMax);
	utils::readCurve(iNode["sizeOverLife"], ioSettings.sizeOverLife);
	readValue(iNode["spinMin"], ioSettings.spinMin);
	readValue(iNode["spinMax"], ioSettings.spinMax);
	readValue(iNode["color"], ioSettings.color);
	utils::readGradient(iNode["colorOverLife"], ioSettings.colorOverLife);
	readValue(iNode["emissive"], ioSettings.emissive);
	readEnum(iNode["blend"], ioSettings.blend);
	readEnum(iNode["space"], ioSettings.space);
	if (iNode["texture"])
		ioSettings.texture =
				renderer::gpu::Texture2D::createFromSerializedForDeserialize(iNode["texture"].as<std::string>());
	readValue(iNode["trailLength"], ioSettings.trailLength);
	ioSettings.trailLength = std::min(ioSettings.trailLength, renderer::g_maxParticleTrail);
	readValue(iNode["trailInterval"], ioSettings.trailInterval);
	readValue(iNode["trailWidth"], ioSettings.trailWidth);
}

}// namespace

void ParticleEmitter::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	const auto& set = settings;
	out << YAML::Key << key() << YAML::BeginMap;
	out << YAML::Key << "playOnStart" << YAML::Value << playOnStart;
	out << YAML::Key << "maxParticles" << YAML::Value << set.maxParticles;
	out << YAML::Key << "rate" << YAML::Value << set.rate;
	out << YAML::Key << "burst" << YAML::Value << set.burst;
	out << YAML::Key << "duration" << YAML::Value << set.duration;
	out << YAML::Key << "loop" << YAML::Value << set.loop;
	out << YAML::Key << "lifetimeMin" << YAML::Value << set.lifetimeMin;
	out << YAML::Key << "lifetimeMax" << YAML::Value << set.lifetimeMax;
	out << YAML::Key << "shape" << YAML::Value << std::string{magic_enum::enum_name(set.shape)};
	out << YAML::Key << "shapeSize" << YAML::Value << set.shapeSize;
	out << YAML::Key << "coneAngle" << YAML::Value << set.coneAngle;
	out << YAML::Key << "speedMin" << YAML::Value << set.speedMin;
	out << YAML::Key << "speedMax" << YAML::Value << set.speedMax;
	out << YAML::Key << "velocity" << YAML::Value << set.velocity;
	out << YAML::Key << "gravity" << YAML::Value << set.gravity;
	out << YAML::Key << "drag" << YAML::Value << set.drag;
	out << YAML::Key << "sizeMin" << YAML::Value << set.sizeMin;
	out << YAML::Key << "sizeMax" << YAML::Value << set.sizeMax;
	utils::emitCurve(out, "sizeOverLife", set.sizeOverLife);
	out << YAML::Key << "spinMin" << YAML::Value << set.spinMin;
	out << YAML::Key << "spinMax" << YAML::Value << set.spinMax;
	out << YAML::Key << "color" << YAML::Value << set.color;
	utils::emitGradient(out, "colorOverLife", set.colorOverLife);
	out << YAML::Key << "emissive" << YAML::Value << set.emissive;
	out << YAML::Key << "blend" << YAML::Value << std::string{magic_enum::enum_name(set.blend)};
	out << YAML::Key << "space" << YAML::Value << std::string{magic_enum::enum_name(set.space)};
	if (set.texture)
		out << YAML::Key << "texture" << YAML::Value << set.texture->getSerializeString();
	out << YAML::Key << "trailLength" << YAML::Value << set.trailLength;
	out << YAML::Key << "trailInterval" << YAML::Value << set.trailInterval;
	out << YAML::Key << "trailWidth" << YAML::Value << set.trailWidth;
	out << YAML::Key << "seed" << YAML::Value << set.seed;
	out << YAML::EndMap;
}

void ParticleEmitter::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	settings = {};
	readValue(node["playOnStart"], playOnStart);
	readEmission(node, settings);
	readLook(node, settings);
	m_slot.system.reset();
}

}// namespace owl::scene::component
