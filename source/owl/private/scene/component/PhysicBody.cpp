/**
 * @file PhysicBody.cpp
 * @author Silmaen
 * @date 1/28/25
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
*/
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/PhysicBody.h"

#include <algorithm>
#include <cstdint>

namespace owl::scene::component {

void PhysicBody::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << std::string(magic_enum::enum_name(body.type));
	// A 2D box, the default, writes neither key.
	if (body.dimension != SceneBody::Dimension::Plane)
		iOut.getImpl()->emitter << YAML::Key << "dimension" << YAML::Value
								<< std::string(magic_enum::enum_name(body.dimension));
	if (body.shape != SceneBody::Shape::Box)
		iOut.getImpl()->emitter << YAML::Key << "shape" << YAML::Value
								<< std::string(magic_enum::enum_name(body.shape));
	iOut.getImpl()->emitter << YAML::Key << "fixedRotation" << YAML::Value << body.fixedRotation;
	// The defaults (a solid body on layer 0 meeting every layer) write no key.
	if (body.sensor)
		iOut.getImpl()->emitter << YAML::Key << "sensor" << YAML::Value << body.sensor;
	if (body.layer != 0)
		iOut.getImpl()->emitter << YAML::Key << "layer" << YAML::Value << static_cast<uint32_t>(body.layer);
	if (body.mask != SceneBody::allLayers)
		iOut.getImpl()->emitter << YAML::Key << "mask" << YAML::Value << static_cast<uint32_t>(body.mask);
	iOut.getImpl()->emitter << YAML::Key << "colliderSize" << YAML::Value << body.colliderSize;
	iOut.getImpl()->emitter << YAML::Key << "density" << YAML::Value << body.density;
	iOut.getImpl()->emitter << YAML::Key << "restitution" << YAML::Value << body.restitution;
	iOut.getImpl()->emitter << YAML::Key << "friction" << YAML::Value << body.friction;
	// Full gravity, fields included, writes neither key.
	if (body.gravityScale < 1.f || body.gravityScale > 1.f)
		iOut.getImpl()->emitter << YAML::Key << "gravityScale" << YAML::Value << body.gravityScale;
	if (!body.gravityFields)
		iOut.getImpl()->emitter << YAML::Key << "gravityFields" << YAML::Value << body.gravityFields;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void PhysicBody::deserialize(const core::Serializer& iNode) {
	body.type = magic_enum::enum_cast<SceneBody::BodyType>(iNode.getImpl()->node["type"].as<std::string>())
						.value_or(SceneBody::BodyType::Static);
	const auto& node = iNode.getImpl()->node;
	body.dimension = node["dimension"]
							 ? magic_enum::enum_cast<SceneBody::Dimension>(node["dimension"].as<std::string>())
									   .value_or(SceneBody::Dimension::Plane)
							 : SceneBody::Dimension::Plane;
	body.shape = node["shape"] ? magic_enum::enum_cast<SceneBody::Shape>(node["shape"].as<std::string>())
										 .value_or(SceneBody::Shape::Box)
							   : SceneBody::Shape::Box;
	body.fixedRotation = iNode.getImpl()->node["fixedRotation"].as<bool>();
	body.sensor = node["sensor"] ? node["sensor"].as<bool>() : false;
	body.layer = node["layer"]
						 ? static_cast<uint8_t>(std::min(node["layer"].as<uint32_t>(), SceneBody::layerCount - 1u))
						 : 0;
	body.mask = node["mask"] ? static_cast<uint8_t>(node["mask"].as<uint32_t>() & 0xffu) : SceneBody::allLayers;
	body.bodyId = 0;
	body.colliderSize = iNode.getImpl()->node["colliderSize"].as<math::vec3f>();
	body.density = iNode.getImpl()->node["density"].as<float>();
	body.restitution = iNode.getImpl()->node["restitution"].as<float>();
	body.friction = iNode.getImpl()->node["friction"].as<float>();
	body.gravityScale = node["gravityScale"] ? node["gravityScale"].as<float>() : 1.f;
	body.gravityFields = node["gravityFields"] ? node["gravityFields"].as<bool>() : true;
}

}// namespace owl::scene::component
