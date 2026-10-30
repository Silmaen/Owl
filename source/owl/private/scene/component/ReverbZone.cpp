/**
 * @file ReverbZone.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/ReverbZone.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace owl::scene::component {

namespace {

constexpr float g_minZoneSize = 0.01f;

auto column(const math::mat4& iMatrix, const size_t iIndex) -> math::vec3 {
	return {iMatrix(0, iIndex), iMatrix(1, iIndex), iMatrix(2, iIndex)};
}

void emitFilter(YAML::Emitter& ioOut, const sound::Filter& iFilter) {
	ioOut << YAML::Flow << YAML::BeginMap;
	ioOut << YAML::Key << "gain" << YAML::Value << iFilter.gain;
	ioOut << YAML::Key << "gainLow" << YAML::Value << iFilter.gainLow;
	ioOut << YAML::Key << "gainHigh" << YAML::Value << iFilter.gainHigh;
	ioOut << YAML::EndMap;
}

void emitReverb(YAML::Emitter& ioOut, const sound::Reverb& iReverb) {
	ioOut << YAML::BeginMap;
	ioOut << YAML::Key << "density" << YAML::Value << iReverb.density;
	ioOut << YAML::Key << "diffusion" << YAML::Value << iReverb.diffusion;
	ioOut << YAML::Key << "gain" << YAML::Value << iReverb.gain;
	ioOut << YAML::Key << "gainHigh" << YAML::Value << iReverb.gainHigh;
	ioOut << YAML::Key << "decayTime" << YAML::Value << iReverb.decayTime;
	ioOut << YAML::Key << "decayHighRatio" << YAML::Value << iReverb.decayHighRatio;
	ioOut << YAML::Key << "reflectionsGain" << YAML::Value << iReverb.reflectionsGain;
	ioOut << YAML::Key << "reflectionsDelay" << YAML::Value << iReverb.reflectionsDelay;
	ioOut << YAML::Key << "lateGain" << YAML::Value << iReverb.lateGain;
	ioOut << YAML::Key << "lateDelay" << YAML::Value << iReverb.lateDelay;
	ioOut << YAML::EndMap;
}

auto readReverb(const core::YamlNode& iNode, const sound::Reverb& iDefault) -> sound::Reverb {
	if (!iNode.isMap())
		return iDefault;
	return sound::Reverb{.density = iNode["density"].as<float>(iDefault.density),
						 .diffusion = iNode["diffusion"].as<float>(iDefault.diffusion),
						 .gain = iNode["gain"].as<float>(iDefault.gain),
						 .gainHigh = iNode["gainHigh"].as<float>(iDefault.gainHigh),
						 .decayTime = iNode["decayTime"].as<float>(iDefault.decayTime),
						 .decayHighRatio = iNode["decayHighRatio"].as<float>(iDefault.decayHighRatio),
						 .reflectionsGain = iNode["reflectionsGain"].as<float>(iDefault.reflectionsGain),
						 .reflectionsDelay = iNode["reflectionsDelay"].as<float>(iDefault.reflectionsDelay),
						 .lateGain = iNode["lateGain"].as<float>(iDefault.lateGain),
						 .lateDelay = iNode["lateDelay"].as<float>(iDefault.lateDelay)}
			.clamped();
}

auto readFilter(const core::YamlNode& iNode) -> sound::Filter {
	if (!iNode.isMap())
		return {};
	return sound::Filter{.gain = iNode["gain"].as<float>(1.f),
						 .gainLow = iNode["gainLow"].as<float>(1.f),
						 .gainHigh = iNode["gainHigh"].as<float>(1.f)}
			.clamped();
}

}// namespace

auto ReverbZone::getReverb() const -> sound::Reverb {
	return preset == sound::ReverbPreset::Custom ? reverb : sound::getReverbPreset(preset);
}

auto ReverbZone::getDistanceOutside(const math::mat4& iWorld, const math::vec3& iPoint) const -> float {
	const math::vec3 center = column(iWorld, 3);
	const math::vec3 offset = iPoint - center;
	const std::array<math::vec3, 3> axes{column(iWorld, 0), column(iWorld, 1), column(iWorld, 2)};
	if (shape == Shape::Sphere) {
		float scale = 0.f;
		for (const auto& axis: axes) scale = std::max(scale, axis.norm());
		return std::max(0.f, offset.norm() - radius * scale);
	}
	// A transform without shear keeps its axes orthogonal: project on each, clamp to the box, measure the rest.
	math::vec3 closest = center;
	for (size_t index = 0; index < axes.size(); ++index) {
		const float lengthSq = axes[index].normSq();
		if (lengthSq <= 1e-12f)
			continue;
		const float along = std::clamp((offset * axes[index]) / lengthSq, -extents[index], extents[index]);
		closest += axes[index] * along;
	}
	return (iPoint - closest).norm();
}

auto ReverbZone::sample(const math::mat4& iWorld, const math::vec3& iPoint) const -> sound::AudioZoneSample {
	return {.reverb = getReverb(),
			.filter = filter,
			.weight = sound::getZoneWeight(getDistanceOutside(iWorld, iPoint), blendDistance)};
}

void ReverbZone::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;
	out << YAML::Key << "shape" << YAML::Value << std::string(magic_enum::enum_name(shape));
	out << YAML::Key << "extents" << YAML::Value << extents;
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "blendDistance" << YAML::Value << blendDistance;
	out << YAML::Key << "preset" << YAML::Value << std::string(sound::getReverbPresetName(preset));
	if (preset == sound::ReverbPreset::Custom) {
		out << YAML::Key << "reverb" << YAML::Value;
		emitReverb(out, reverb);
	}
	if (!filter.isNeutral()) {
		out << YAML::Key << "filter" << YAML::Value;
		emitFilter(out, filter);
	}
	out << YAML::EndMap;
}

void ReverbZone::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["shape"]) {
		if (const auto parsed = magic_enum::enum_cast<Shape>(node["shape"].as<std::string>()); parsed.has_value())
			shape = parsed.value();
	}
	if (node["extents"]) {
		const auto read = node["extents"].as<math::vec3>();
		extents = {std::max(read.x(), g_minZoneSize), std::max(read.y(), g_minZoneSize),
				   std::max(read.z(), g_minZoneSize)};
	}
	if (node["radius"])
		radius = std::max(node["radius"].as<float>(), g_minZoneSize);
	if (node["blendDistance"])
		blendDistance = std::max(node["blendDistance"].as<float>(), 0.f);
	if (node["preset"])
		preset = sound::parseReverbPreset(node["preset"].as<std::string>()).value_or(sound::ReverbPreset::Generic);
	reverb = readReverb(node["reverb"], sound::getReverbPreset(preset));
	filter = readFilter(node["filter"]);
}

}// namespace owl::scene::component
