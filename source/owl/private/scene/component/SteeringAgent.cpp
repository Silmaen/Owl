/**
 * @file SteeringAgent.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/SteeringAgent.h"

#include <magic_enum/magic_enum.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace owl::scene::component {

namespace {

void emitWeights(YAML::Emitter& ioOut, const SteeringWeights& iWeights) {
	ioOut << YAML::Key << "weights" << YAML::Value << YAML::Flow << YAML::BeginMap;
	const auto emit = [&ioOut](const char* iKey, const float iValue) -> void {
		ioOut << YAML::Key << iKey << YAML::Value << iValue;
	};
	emit("seek", iWeights.seek);
	emit("flee", iWeights.flee);
	emit("arrive", iWeights.arrive);
	emit("pursue", iWeights.pursue);
	emit("evade", iWeights.evade);
	emit("wander", iWeights.wander);
	emit("separation", iWeights.separation);
	emit("alignment", iWeights.alignment);
	emit("cohesion", iWeights.cohesion);
	emit("avoidance", iWeights.avoidance);
	emit("formation", iWeights.formation);
	ioOut << YAML::EndMap;
}

auto readWeights(const core::YamlNode& iNode) -> SteeringWeights {
	SteeringWeights weights;
	if (!iNode.isMap())
		return weights;
	weights.seek = iNode["seek"].as<float>(weights.seek);
	weights.flee = iNode["flee"].as<float>(weights.flee);
	weights.arrive = iNode["arrive"].as<float>(weights.arrive);
	weights.pursue = iNode["pursue"].as<float>(weights.pursue);
	weights.evade = iNode["evade"].as<float>(weights.evade);
	weights.wander = iNode["wander"].as<float>(weights.wander);
	weights.separation = iNode["separation"].as<float>(weights.separation);
	weights.alignment = iNode["alignment"].as<float>(weights.alignment);
	weights.cohesion = iNode["cohesion"].as<float>(weights.cohesion);
	weights.avoidance = iNode["avoidance"].as<float>(weights.avoidance);
	weights.formation = iNode["formation"].as<float>(weights.formation);
	return weights;
}

}// namespace

auto SteeringWeights::find(const std::string_view iName) -> float* {
	const std::array<std::pair<std::string_view, float*>, 11> weights{{{"seek", &seek},
																	   {"flee", &flee},
																	   {"arrive", &arrive},
																	   {"pursue", &pursue},
																	   {"evade", &evade},
																	   {"wander", &wander},
																	   {"separation", &separation},
																	   {"alignment", &alignment},
																	   {"cohesion", &cohesion},
																	   {"avoidance", &avoidance},
																	   {"formation", &formation}}};
	for (const auto& [name, weight]: weights)
		if (name == iName)
			return weight;
	return nullptr;
}

void SteeringAgent::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// SteeringAgent
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "plane" << YAML::Value << std::string(magic_enum::enum_name(plane));
	out << YAML::Key << "maxSpeed" << YAML::Value << maxSpeed;
	out << YAML::Key << "maxAcceleration" << YAML::Value << maxAcceleration;
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "turn" << YAML::Value << turn;
	out << YAML::Key << "target" << YAML::Value << static_cast<uint64_t>(target);
	out << YAML::Key << "targetPoint" << YAML::Value << targetPoint;
	emitWeights(out, weights);
	out << YAML::Key << "slowRadius" << YAML::Value << slowRadius;
	out << YAML::Key << "panicDistance" << YAML::Value << panicDistance;
	out << YAML::Key << "prediction" << YAML::Value << prediction;
	out << YAML::Key << "wander" << YAML::Value << YAML::Flow << YAML::BeginMap;
	out << YAML::Key << "radius" << YAML::Value << wanderSettings.radius;
	out << YAML::Key << "distance" << YAML::Value << wanderSettings.distance;
	out << YAML::Key << "jitter" << YAML::Value << wanderSettings.jitter;
	out << YAML::EndMap;
	out << YAML::Key << "flock" << YAML::Value << flock;
	out << YAML::Key << "neighbourRadius" << YAML::Value << neighbourRadius;
	out << YAML::Key << "separationDistance" << YAML::Value << separationDistance;
	out << YAML::Key << "lookAhead" << YAML::Value << lookAhead;
	out << YAML::Key << "leader" << YAML::Value << static_cast<uint64_t>(leader);
	out << YAML::Key << "slot" << YAML::Value << slot;
	out << YAML::EndMap;// SteeringAgent
}

void SteeringAgent::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	enabled = node["enabled"].as<bool>(enabled);
	if (node["plane"]) {
		if (const auto parsed = magic_enum::enum_cast<ai::SteeringPlane>(node["plane"].as<std::string>());
			parsed.has_value())
			plane = parsed.value();
	}
	maxSpeed = node["maxSpeed"].as<float>(maxSpeed);
	maxAcceleration = node["maxAcceleration"].as<float>(maxAcceleration);
	radius = node["radius"].as<float>(radius);
	turn = node["turn"].as<bool>(turn);
	if (node["target"])
		target = core::UUID{node["target"].as<uint64_t>()};
	if (node["targetPoint"])
		targetPoint = node["targetPoint"].as<math::vec3>();
	weights = readWeights(node["weights"]);
	slowRadius = node["slowRadius"].as<float>(slowRadius);
	panicDistance = node["panicDistance"].as<float>(panicDistance);
	prediction = node["prediction"].as<float>(prediction);
	if (const auto wanderNode = node["wander"]; wanderNode.isMap()) {
		wanderSettings.radius = wanderNode["radius"].as<float>(wanderSettings.radius);
		wanderSettings.distance = wanderNode["distance"].as<float>(wanderSettings.distance);
		wanderSettings.jitter = wanderNode["jitter"].as<float>(wanderSettings.jitter);
	}
	flock = node["flock"].as<std::string>(flock);
	neighbourRadius = node["neighbourRadius"].as<float>(neighbourRadius);
	separationDistance = node["separationDistance"].as<float>(separationDistance);
	lookAhead = node["lookAhead"].as<float>(lookAhead);
	if (node["leader"])
		leader = core::UUID{node["leader"].as<uint64_t>()};
	if (node["slot"])
		slot = node["slot"].as<math::vec3>();
	m_velocity = {0.f, 0.f, 0.f};
	m_wander = {};
	m_targetTracked = core::UUID{0};
}

}// namespace owl::scene::component
