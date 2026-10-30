/**
 * @file Spline.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Spline.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace owl::scene::component {

namespace {

void readPoints(const core::YamlNode& iList, std::vector<math::SplinePoint>& oPoints) {
	oPoints.clear();
	if (!iList.isSequence())
		return;
	for (const auto item: iList) {
		math::SplinePoint point;
		if (item.isSequence()) {
			point.position = item.as<math::vec3>();
		} else {
			if (item["position"])
				point.position = item["position"].as<math::vec3>();
			if (item["tangentIn"])
				point.tangentIn = item["tangentIn"].as<math::vec3>();
			if (item["tangentOut"])
				point.tangentOut = item["tangentOut"].as<math::vec3>();
		}
		oPoints.push_back(point);
	}
}

void readLanes(const core::YamlNode& iList, std::vector<Spline::Lane>& oLanes) {
	oLanes.clear();
	if (!iList.isSequence())
		return;
	for (const auto item: iList)
		oLanes.push_back({.name = item["name"].as<std::string>(std::string{}),
						  .offset = item["offset"].as<float>(0.f),
						  .height = item["height"].as<float>(0.f)});
}

}// namespace

void Spline::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Spline
	out << YAML::Key << "interpolation" << YAML::Value << std::string(magic_enum::enum_name(curve.getInterpolation()));
	out << YAML::Key << "closed" << YAML::Value << curve.isClosed();
	out << YAML::Key << "resolution" << YAML::Value << curve.getResolution();
	out << YAML::Key << "up" << YAML::Value << up;
	const bool bezier = curve.getInterpolation() == math::SplineInterpolation::Bezier;
	out << YAML::Key << "points" << YAML::Value << YAML::BeginSeq;
	for (const auto& point: curve.getPoints()) {
		if (!bezier) {
			out << point.position;
			continue;
		}
		out << YAML::Flow << YAML::BeginMap;
		out << YAML::Key << "position" << YAML::Value << point.position;
		out << YAML::Key << "tangentIn" << YAML::Value << point.tangentIn;
		out << YAML::Key << "tangentOut" << YAML::Value << point.tangentOut;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "lanes" << YAML::Value << YAML::BeginSeq;
	for (const auto& lane: lanes) {
		out << YAML::Flow << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << lane.name;
		out << YAML::Key << "offset" << YAML::Value << lane.offset;
		out << YAML::Key << "height" << YAML::Value << lane.height;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;// Spline
}

void Spline::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["interpolation"]) {
		if (const auto parsed =
					magic_enum::enum_cast<math::SplineInterpolation>(node["interpolation"].as<std::string>());
			parsed.has_value())
			curve.setInterpolation(parsed.value());
	}
	if (node["closed"])
		curve.setClosed(node["closed"].as<bool>());
	if (node["resolution"])
		curve.setResolution(node["resolution"].as<uint32_t>());
	if (node["up"])
		up = node["up"].as<math::vec3>();
	std::vector<math::SplinePoint> points;
	readPoints(node["points"], points);
	curve.setPoints(std::move(points));
	readLanes(node["lanes"], lanes);
}

auto Spline::findLane(const std::string_view iName) const -> const Lane* {
	const auto found = std::ranges::find(lanes, iName, &Lane::name);
	return found == lanes.end() ? nullptr : &*found;
}

auto Spline::getLateral(const math::vec3& iTangent) const -> math::vec3 {
	const math::vec3 lateral = iTangent ^ up;
	const float length = lateral.norm();
	return length > std::numeric_limits<float>::epsilon() ? lateral * (1.f / length) : math::vec3{0.f, 0.f, 0.f};
}

auto Spline::sample(const float iDistance, const std::string_view iLane, const float iOffset) const
		-> math::SplineSample {
	auto result = curve.sampleAtDistance(iDistance);
	float offset = iOffset;
	float height = 0.f;
	if (const auto* lane = iLane.empty() ? nullptr : findLane(iLane); lane != nullptr) {
		offset += lane->offset;
		height = lane->height;
	}
	const float upLength = up.norm();
	const math::vec3 upUnit = upLength > 0.f ? up * (1.f / upLength) : math::vec3{0.f, 0.f, 0.f};
	result.position = result.position + getLateral(result.tangent) * offset + upUnit * height;
	return result;
}

void SplineFollower::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// SplineFollower
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "spline" << YAML::Value << static_cast<uint64_t>(spline);
	out << YAML::Key << "lane" << YAML::Value << lane;
	out << YAML::Key << "offset" << YAML::Value << offset;
	out << YAML::Key << "speed" << YAML::Value << speed;
	out << YAML::Key << "startDistance" << YAML::Value << startDistance;
	out << YAML::Key << "mode" << YAML::Value << std::string(magic_enum::enum_name(mode));
	out << YAML::Key << "orient" << YAML::Value << orient;
	out << YAML::EndMap;// SplineFollower
}

void SplineFollower::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["spline"])
		spline = core::UUID{node["spline"].as<uint64_t>()};
	if (node["lane"])
		lane = node["lane"].as<std::string>();
	if (node["offset"])
		offset = node["offset"].as<float>();
	if (node["speed"])
		speed = node["speed"].as<float>();
	if (node["startDistance"])
		startDistance = node["startDistance"].as<float>();
	if (node["mode"]) {
		if (const auto parsed = magic_enum::enum_cast<Mode>(node["mode"].as<std::string>()); parsed.has_value())
			mode = parsed.value();
	}
	if (node["orient"])
		orient = node["orient"].as<bool>();
	m_started = false;
}

}// namespace owl::scene::component
