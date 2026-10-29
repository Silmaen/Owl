/**
 * @file Trail.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/component/Trail.h"

#include "ParticleYaml.h"
#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "renderer/RendererParticles.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace owl::scene::component {

auto Trail::bakeTables() const -> renderer::ParticleTables {
	renderer::ParticleTables tables;
	for (uint32_t index = 0; index < renderer::g_particleTableSize; ++index) {
		const float along = static_cast<float>(index) / static_cast<float>(renderer::g_particleTableSize - 1);
		const math::vec4 shade =
				colorOverLength.keys.empty() ? math::vec4{1.f, 1.f, 1.f, 1.f - along} : colorOverLength.evaluate(along);
		tables.color[index] =
				math::vec4{color.x() * shade.x(), color.y() * shade.y(), color.z() * shade.z(), color.w() * shade.w()};
		tables.size[index] = widthOverLength.empty() ? 1.f - along : widthOverLength.evaluate(along);
	}
	return tables;
}

void Trail::record(const math::vec3& iPosition, const float iSeconds) {
	if (!enabled) {
		m_points.clear();
		return;
	}
	const auto most = static_cast<size_t>(std::clamp(maxPoints, 2u, renderer::RendererParticles::g_maxTrailPoints));
	if (m_points.capacity() < most + 1)
		m_points.reserve(most + 1);
	for (auto& point: m_points) point.w() += iSeconds;
	while (m_points.size() > 2 && m_points.back().w() > lifetime) m_points.pop_back();
	const math::vec4 head{iPosition.x(), iPosition.y(), iPosition.z(), 0.f};
	if (m_points.empty())
		m_points.push_back(head);
	m_points.front() = head;
	const bool moved =
			m_points.size() < 2 || (iPosition - math::vec3{m_points[1].x(), m_points[1].y(), m_points[1].z()}).norm() >=
										   std::max(minDistance, 1e-4f);
	if (moved)
		m_points.insert(m_points.begin(), head);
	if (m_points.size() > most)
		m_points.resize(most);
	if (m_points.size() == 2 && m_points.back().w() > lifetime)
		m_points.back() = head;
}

void Trail::shiftOrigin(const math::vec3& iShift) {
	for (auto& point: m_points)
		point = math::vec4{point.x() - iShift.x(), point.y() - iShift.y(), point.z() - iShift.z(), point.w()};
}

void Trail::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key() << YAML::BeginMap;
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "lifetime" << YAML::Value << lifetime;
	out << YAML::Key << "minDistance" << YAML::Value << minDistance;
	out << YAML::Key << "maxPoints" << YAML::Value << maxPoints;
	out << YAML::Key << "width" << YAML::Value << width;
	utils::emitCurve(out, "widthOverLength", widthOverLength);
	out << YAML::Key << "color" << YAML::Value << color;
	utils::emitGradient(out, "colorOverLength", colorOverLength);
	out << YAML::Key << "emissive" << YAML::Value << emissive;
	out << YAML::Key << "blend" << YAML::Value << std::string{magic_enum::enum_name(blend)};
	out << YAML::EndMap;
}

void Trail::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["lifetime"])
		lifetime = std::max(node["lifetime"].as<float>(), 0.f);
	if (node["minDistance"])
		minDistance = std::max(node["minDistance"].as<float>(), 0.f);
	if (node["maxPoints"])
		maxPoints = std::clamp(node["maxPoints"].as<uint32_t>(), 2u, renderer::RendererParticles::g_maxTrailPoints);
	if (node["width"])
		width = node["width"].as<float>();
	utils::readCurve(node["widthOverLength"], widthOverLength);
	if (node["color"])
		color = node["color"].as<math::vec4>();
	utils::readGradient(node["colorOverLength"], colorOverLength);
	if (node["emissive"])
		emissive = node["emissive"].as<float>();
	if (node["blend"]) {
		if (const auto parsed = magic_enum::enum_cast<renderer::ParticleBlend>(node["blend"].as<std::string>());
			parsed.has_value())
			blend = parsed.value();
	}
	m_points.clear();
}

}// namespace owl::scene::component
