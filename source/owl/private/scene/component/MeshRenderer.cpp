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

#include <string>

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
}

void MeshRenderer::setMeshPath(const std::filesystem::path& iPath) {
	meshPath = iPath;
	mesh.reset();
}

}// namespace owl::scene::component
