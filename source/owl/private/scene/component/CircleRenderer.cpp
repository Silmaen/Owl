
/**
 * @file CircleRenderer.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/CircleRenderer.h"

namespace owl::scene::component {


void CircleRenderer::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// CircleRenderer
	iOut.getImpl()->emitter << YAML::Key << "color" << YAML::Value << color;
	iOut.getImpl()->emitter << YAML::Key << "thickness" << YAML::Value << thickness;
	iOut.getImpl()->emitter << YAML::Key << "fade" << YAML::Value << fade;
	iOut.getImpl()->emitter << YAML::EndMap;// CircleRenderer
}

/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
void CircleRenderer::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["color"])
		color = iNode.getImpl()->node["color"].as<math::vec4>();
	if (iNode.getImpl()->node["thickness"])
		thickness = iNode.getImpl()->node["thickness"].as<float>();
	if (iNode.getImpl()->node["fade"])
		fade = iNode.getImpl()->node["fade"].as<float>();
}


}// namespace owl::scene::component
