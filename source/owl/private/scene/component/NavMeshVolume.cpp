/**
 * @file NavMeshVolume.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/NavMeshVolume.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace owl::scene::component {

namespace {

void emitSettings(YAML::Emitter& ioOut, const nav::NavMeshSettings& iSettings) {
	ioOut << YAML::Key << "settings" << YAML::Value << YAML::BeginMap;
	ioOut << YAML::Key << "cellSize" << YAML::Value << iSettings.cellSize;
	ioOut << YAML::Key << "cellHeight" << YAML::Value << iSettings.cellHeight;
	ioOut << YAML::Key << "agentHeight" << YAML::Value << iSettings.agentHeight;
	ioOut << YAML::Key << "agentRadius" << YAML::Value << iSettings.agentRadius;
	ioOut << YAML::Key << "maxClimb" << YAML::Value << iSettings.maxClimb;
	ioOut << YAML::Key << "maxSlope" << YAML::Value << iSettings.maxSlope;
	ioOut << YAML::Key << "minRegionArea" << YAML::Value << iSettings.minRegionArea;
	ioOut << YAML::Key << "maxEdgeError" << YAML::Value << iSettings.maxEdgeError;
	ioOut << YAML::EndMap;
}

auto readSettings(const core::YamlNode& iNode) -> nav::NavMeshSettings {
	nav::NavMeshSettings settings;
	if (!iNode.isMap())
		return settings;
	settings.cellSize = iNode["cellSize"].as<float>(settings.cellSize);
	settings.cellHeight = iNode["cellHeight"].as<float>(settings.cellHeight);
	settings.agentHeight = iNode["agentHeight"].as<float>(settings.agentHeight);
	settings.agentRadius = iNode["agentRadius"].as<float>(settings.agentRadius);
	settings.maxClimb = iNode["maxClimb"].as<float>(settings.maxClimb);
	settings.maxSlope = iNode["maxSlope"].as<float>(settings.maxSlope);
	settings.minRegionArea = iNode["minRegionArea"].as<float>(settings.minRegionArea);
	settings.maxEdgeError = iNode["maxEdgeError"].as<float>(settings.maxEdgeError);
	return settings;
}

}// namespace

void NavMeshVolume::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// NavMeshVolume
	out << YAML::Key << "size" << YAML::Value << size;
	emitSettings(out, settings);
	out << YAML::Key << "sources" << YAML::Value << std::string(magic_enum::enum_name(sources));
	// Flat lists keep a baked mesh of thousands of triangles on a few lines.
	out << YAML::Key << "vertices" << YAML::Value << YAML::Flow << YAML::BeginSeq;
	for (const auto& vertex: vertices) out << vertex.x() << vertex.y() << vertex.z();
	out << YAML::EndSeq;
	out << YAML::Key << "indices" << YAML::Value << YAML::Flow << YAML::BeginSeq;
	for (const auto index: indices) out << index;
	out << YAML::EndSeq;
	out << YAML::EndMap;// NavMeshVolume
}

void NavMeshVolume::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["size"])
		size = node["size"].as<math::vec3>();
	settings = readSettings(node["settings"]);
	if (node["sources"]) {
		if (const auto parsed = magic_enum::enum_cast<Sources>(node["sources"].as<std::string>()); parsed.has_value())
			sources = parsed.value();
	}
	vertices.clear();
	if (const auto list = node["vertices"]; list.isSequence()) {
		std::vector<float> values;
		for (const auto value: list) values.push_back(value.as<float>());
		for (size_t index = 0; index + 2 < values.size(); index += 3)
			vertices.emplace_back(values[index], values[index + 1], values[index + 2]);
	}
	indices.clear();
	if (const auto list = node["indices"]; list.isSequence()) {
		for (const auto value: list) indices.push_back(value.as<uint32_t>());
	}
	m_meshBuilt = false;
}

auto NavMeshVolume::getMesh() const -> const nav::NavMesh& {
	if (!m_meshBuilt || m_meshVertices != vertices || m_meshIndices != indices) {
		m_mesh.setTriangles(vertices, indices);
		m_meshVertices = vertices;
		m_meshIndices = indices;
		m_meshBuilt = true;
		if (!m_links.empty())
			m_mesh.setLinks(m_links);
	}
	return m_mesh;
}

void NavMeshVolume::setLinks(const std::vector<nav::NavMeshLink>& iLinks) const {
	std::ignore = getMesh();
	if (iLinks == m_links)
		return;
	m_links = iLinks;
	m_mesh.setLinks(m_links);
}

void NavAgent::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// NavAgent
	out << YAML::Key << "volume" << YAML::Value << static_cast<uint64_t>(volume);
	out << YAML::Key << "speed" << YAML::Value << speed;
	out << YAML::Key << "acceleration" << YAML::Value << acceleration;
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "stoppingDistance" << YAML::Value << stoppingDistance;
	out << YAML::Key << "avoidance" << YAML::Value << avoidance;
	out << YAML::Key << "turn" << YAML::Value << turn;
	out << YAML::Key << "speedParameter" << YAML::Value << speedParameter;
	out << YAML::EndMap;// NavAgent
}

void NavAgent::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["volume"])
		volume = core::UUID{node["volume"].as<uint64_t>()};
	speed = node["speed"].as<float>(speed);
	acceleration = node["acceleration"].as<float>(acceleration);
	radius = node["radius"].as<float>(radius);
	stoppingDistance = node["stoppingDistance"].as<float>(stoppingDistance);
	avoidance = node["avoidance"].as<bool>(avoidance);
	turn = node["turn"].as<bool>(turn);
	speedParameter = node["speedParameter"].as<std::string>(speedParameter);
	stop();
	m_velocity = {};
	m_triangle = -1;
}

void NavObstacle::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// NavObstacle
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "height" << YAML::Value << height;
	out << YAML::EndMap;// NavObstacle
}

void NavObstacle::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	radius = node["radius"].as<float>(radius);
	height = node["height"].as<float>(height);
	m_started = false;
	m_velocity = {};
}

void NavLink::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// NavLink
	out << YAML::Key << "end" << YAML::Value << end;
	out << YAML::Key << "bidirectional" << YAML::Value << bidirectional;
	out << YAML::Key << "costScale" << YAML::Value << costScale;
	out << YAML::EndMap;// NavLink
}

void NavLink::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["end"])
		end = node["end"].as<math::vec3>();
	bidirectional = node["bidirectional"].as<bool>(bidirectional);
	costScale = node["costScale"].as<float>(costScale);
}

}// namespace owl::scene::component
