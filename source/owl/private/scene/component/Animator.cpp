/**
 * @file Animator.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "core/YamlNode.h"
#include "scene/component/Animator.h"

#include <magic_enum/magic_enum.hpp>

#include <string>
#include <utility>

namespace owl::scene::component {

namespace {

template<typename Enum>
auto readEnum(const core::YamlNode& iNode, const Enum iDefault) -> Enum {
	if (!iNode.isScalar())
		return iDefault;
	return magic_enum::enum_cast<Enum>(iNode.as<std::string>(std::string{})).value_or(iDefault);
}

void emitMachine(YAML::Emitter& ioOut, const data::animation::StateMachine& iMachine) {
	ioOut << YAML::Key << "entry" << YAML::Value << iMachine.entryState;
	ioOut << YAML::Key << "parameters" << YAML::Value << YAML::BeginSeq;
	for (const auto& parameter: iMachine.parameters) {
		ioOut << YAML::Flow << YAML::BeginMap << YAML::Key << "name" << YAML::Value << parameter.name;
		ioOut << YAML::Key << "type" << YAML::Value << std::string{magic_enum::enum_name(parameter.type)};
		ioOut << YAML::Key << "value" << YAML::Value << parameter.value << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
	ioOut << YAML::Key << "states" << YAML::Value << YAML::BeginSeq;
	for (const auto& state: iMachine.states) {
		ioOut << YAML::Flow << YAML::BeginMap << YAML::Key << "name" << YAML::Value << state.name;
		ioOut << YAML::Key << "clip" << YAML::Value << state.clip;
		ioOut << YAML::Key << "speed" << YAML::Value << state.speed;
		ioOut << YAML::Key << "loop" << YAML::Value << state.loop << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
	ioOut << YAML::Key << "transitions" << YAML::Value << YAML::BeginSeq;
	for (const auto& transition: iMachine.transitions) {
		ioOut << YAML::BeginMap << YAML::Key << "from" << YAML::Value << transition.from;
		ioOut << YAML::Key << "to" << YAML::Value << transition.to;
		ioOut << YAML::Key << "duration" << YAML::Value << transition.duration;
		ioOut << YAML::Key << "exitTime" << YAML::Value << transition.exitTime;
		ioOut << YAML::Key << "conditions" << YAML::Value << YAML::BeginSeq;
		for (const auto& condition: transition.conditions) {
			ioOut << YAML::Flow << YAML::BeginMap << YAML::Key << "parameter" << YAML::Value << condition.parameter;
			ioOut << YAML::Key << "op" << YAML::Value << std::string{magic_enum::enum_name(condition.op)};
			ioOut << YAML::Key << "threshold" << YAML::Value << condition.threshold << YAML::EndMap;
		}
		ioOut << YAML::EndSeq << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
}

auto readTransition(const core::YamlNode& iNode) -> data::animation::StateTransition {
	data::animation::StateTransition transition;
	transition.from = iNode["from"].as<std::string>(std::string{});
	transition.to = iNode["to"].as<std::string>(std::string{});
	transition.duration = iNode["duration"].as<float>(transition.duration);
	transition.exitTime = iNode["exitTime"].as<float>(transition.exitTime);
	if (const auto conditions = iNode["conditions"]; conditions.isSequence()) {
		for (const auto item: conditions) {
			data::animation::TransitionCondition condition;
			condition.parameter = item["parameter"].as<std::string>(std::string{});
			condition.op = readEnum(item["op"], data::animation::ConditionOp::Greater);
			condition.threshold = item["threshold"].as<float>(0.f);
			transition.conditions.push_back(std::move(condition));
		}
	}
	return transition;
}

auto readMachine(const core::YamlNode& iNode) -> data::animation::StateMachine {
	data::animation::StateMachine machine;
	machine.entryState = iNode["entry"].as<std::string>(std::string{});
	if (const auto parameters = iNode["parameters"]; parameters.isSequence()) {
		for (const auto item: parameters) {
			data::animation::StateParameter parameter;
			parameter.name = item["name"].as<std::string>(std::string{});
			parameter.type = readEnum(item["type"], data::animation::ParameterType::Float);
			parameter.value = item["value"].as<float>(0.f);
			machine.parameters.push_back(std::move(parameter));
		}
	}
	if (const auto states = iNode["states"]; states.isSequence()) {
		for (const auto item: states) {
			data::animation::AnimationState state;
			state.name = item["name"].as<std::string>(std::string{});
			state.clip = item["clip"].as<std::string>(std::string{});
			state.speed = item["speed"].as<float>(1.f);
			state.loop = item["loop"].as<bool>(true);
			machine.states.push_back(std::move(state));
		}
	}
	if (const auto transitions = iNode["transitions"]; transitions.isSequence()) {
		for (const auto item: transitions) machine.transitions.push_back(readTransition(item));
	}
	return machine;
}

}// namespace

void Animator::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Animator
	out << YAML::Key << "playing" << YAML::Value << playing;
	out << YAML::Key << "speed" << YAML::Value << speed;
	if (preview)
		out << YAML::Key << "preview" << YAML::Value << preview;
	emitMachine(out, machine);
	out << YAML::EndMap;// Animator
}

void Animator::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	playing = node["playing"].as<bool>(playing);
	speed = node["speed"].as<float>(speed);
	preview = node["preview"].as<bool>(false);
	machine = readMachine(node);
	invalidate();
}

void Animator::invalidate() {
	player.invalidate();
	pose.clear();
	boneMatrices.clear();
	skinMatrices.clear();
	source = nullptr;
}

}// namespace owl::scene::component
