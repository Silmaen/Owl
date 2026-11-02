/**
 * @file WalkArea.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/WalkArea.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace owl::scene::component {

namespace {

void emitRing(YAML::Emitter& ioOut, const std::vector<math::vec2>& iRing) {
	ioOut << YAML::Flow << YAML::BeginSeq;
	for (const auto& point: iRing) ioOut << point;
	ioOut << YAML::EndSeq;
}

auto readRing(const core::YamlNode& iNode) -> std::vector<math::vec2> {
	std::vector<math::vec2> ring;
	if (!iNode.isSequence())
		return ring;
	for (const auto point: iNode) ring.push_back(point.as<math::vec2>());
	return ring;
}

}// namespace

void WalkArea::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// WalkArea
	out << YAML::Key << "polygons" << YAML::Value << YAML::BeginSeq;
	for (const auto& polygon: polygons) {
		out << YAML::BeginMap;
		out << YAML::Key << "outline" << YAML::Value;
		emitRing(out, polygon.outline);
		out << YAML::Key << "holes" << YAML::Value << YAML::BeginSeq;
		for (const auto& hole: polygon.holes) emitRing(out, hole);
		out << YAML::EndSeq;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "depthScale" << YAML::Value << depthScale;
	out << YAML::Key << "nearY" << YAML::Value << nearY;
	out << YAML::Key << "nearScale" << YAML::Value << nearScale;
	out << YAML::Key << "farY" << YAML::Value << farY;
	out << YAML::Key << "farScale" << YAML::Value << farScale;
	out << YAML::Key << "depthSort" << YAML::Value << depthSort;
	out << YAML::Key << "depthPerUnit" << YAML::Value << depthPerUnit;
	out << YAML::EndMap;// WalkArea
}

void WalkArea::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	polygons.clear();
	if (const auto list = node["polygons"]; list.isSequence()) {
		for (const auto item: list) {
			nav::WalkPolygon polygon;
			polygon.outline = readRing(item["outline"]);
			if (const auto holes = item["holes"]; holes.isSequence()) {
				for (const auto hole: holes) polygon.holes.push_back(readRing(hole));
			}
			polygons.push_back(std::move(polygon));
		}
	}
	depthScale = node["depthScale"].as<bool>(depthScale);
	nearY = node["nearY"].as<float>(nearY);
	nearScale = node["nearScale"].as<float>(nearScale);
	farY = node["farY"].as<float>(farY);
	farScale = node["farScale"].as<float>(farScale);
	depthSort = node["depthSort"].as<bool>(depthSort);
	depthPerUnit = node["depthPerUnit"].as<float>(depthPerUnit);
	m_meshBuilt = false;
}

auto WalkArea::getMesh() const -> const nav::WalkMesh& {
	if (!m_meshBuilt || m_meshSource != polygons) {
		m_mesh.build(polygons);
		m_meshSource = polygons;
		m_meshBuilt = true;
	}
	return m_mesh;
}

auto WalkArea::getScaleAt(const float iY) const -> float {
	if (!depthScale)
		return 1.f;
	const float span = farY - nearY;
	if (std::abs(span) <= 1e-6f)
		return std::max(0.01f, nearScale);
	const float share = (iY - nearY) / span;
	return std::max(0.01f, nearScale + (farScale - nearScale) * share);
}

auto WalkArea::getDepthAt(const float iY) const -> float { return depthSort ? (nearY - iY) * depthPerUnit : 0.f; }

auto WalkArea::getRing(const size_t iPolygon, const size_t iRing) -> std::vector<math::vec2>* {
	if (iPolygon >= polygons.size())
		return nullptr;
	auto& polygon = polygons[iPolygon];
	if (iRing == 0)
		return &polygon.outline;
	return iRing - 1 < polygon.holes.size() ? &polygon.holes[iRing - 1] : nullptr;
}

auto WalkArea::getRing(const size_t iPolygon, const size_t iRing) const -> const std::vector<math::vec2>* {
	if (iPolygon >= polygons.size())
		return nullptr;
	const auto& polygon = polygons[iPolygon];
	if (iRing == 0)
		return &polygon.outline;
	return iRing - 1 < polygon.holes.size() ? &polygon.holes[iRing - 1] : nullptr;
}

void WalkAgent::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// WalkAgent
	out << YAML::Key << "area" << YAML::Value << static_cast<uint64_t>(area);
	out << YAML::Key << "speed" << YAML::Value << speed;
	out << YAML::Key << "scaleSpeed" << YAML::Value << scaleSpeed;
	out << YAML::Key << "facing" << YAML::Value << std::string(magic_enum::enum_name(facing));
	out << YAML::Key << "speedParameter" << YAML::Value << speedParameter;
	out << YAML::EndMap;// WalkAgent
}

void WalkAgent::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["area"])
		area = core::UUID{node["area"].as<uint64_t>()};
	speed = node["speed"].as<float>(speed);
	scaleSpeed = node["scaleSpeed"].as<bool>(scaleSpeed);
	if (node["facing"]) {
		if (const auto parsed = magic_enum::enum_cast<Facing>(node["facing"].as<std::string>()); parsed.has_value())
			facing = parsed.value();
	}
	speedParameter = node["speedParameter"].as<std::string>(speedParameter);
	stop();
	m_started = false;
}

void WalkBehind::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// WalkBehind
	out << YAML::Key << "area" << YAML::Value << static_cast<uint64_t>(area);
	out << YAML::Key << "baseline" << YAML::Value << baseline;
	out << YAML::EndMap;// WalkBehind
}

void WalkBehind::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["area"])
		area = core::UUID{node["area"].as<uint64_t>()};
	baseline = node["baseline"].as<float>(baseline);
}

}// namespace owl::scene::component
