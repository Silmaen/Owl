/**
 * @file MeshRenderer.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "renderer/MeshCache.h"
#include "scene/component/MeshRenderer.h"

#include <algorithm>
#include <string>
#include <utility>

namespace owl::scene::component {

void MeshRenderer::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// MeshRenderer
	out << YAML::Key << "mesh" << YAML::Value << meshPath.generic_string();
	out << YAML::Key << "color" << YAML::Value << color;
	if (texture)
		out << YAML::Key << "texture" << YAML::Value << texture->getSerializeString();
	out << YAML::Key << "transparent" << YAML::Value << transparent;
	const bool anySlot = std::ranges::any_of(materials, [](const MaterialSlot& iSlot) -> bool {
		return !iSlot.material.empty() || !iSlot.overrides.isEmpty();
	});
	if (anySlot) {
		out << YAML::Key << "materials" << YAML::Value << YAML::BeginSeq;
		for (const auto& slot: materials) {
			out << YAML::BeginMap;
			if (!slot.material.empty())
				out << YAML::Key << "material" << YAML::Value << slot.material.generic_string();
			if (!slot.overrides.isEmpty())
				out << YAML::Key << "overrides" << YAML::Value << YAML::Load(slot.overrides.toYaml());
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}
	out << YAML::EndMap;// MeshRenderer
}

void MeshRenderer::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["mesh"])
		setMeshPath(node["mesh"].as<std::string>());
	if (node["color"])
		color = node["color"].as<math::vec4>();
	if (node["texture"])
		texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(node["texture"].as<std::string>());
	if (node["transparent"])
		transparent = node["transparent"].as<bool>();
	materials.clear();
	if (const auto slots = node["materials"]; slots && slots.isSequence()) {
		for (const auto item: slots) {
			MaterialSlot slot;
			if (const auto path = item["material"]; path && path.isScalar())
				slot.material = path.as<std::string>();
			if (const auto overrides = item["overrides"]; overrides && overrides.isMap())
				slot.overrides = renderer::MaterialOverrides::fromYaml(overrides.emit());
			materials.push_back(std::move(slot));
		}
	}
}

void MeshRenderer::setMeshPath(const std::filesystem::path& iPath) {
	meshPath = iPath;
	mesh.reset();
	for (auto& slot: materials) slot.invalidate();
}

}// namespace owl::scene::component
