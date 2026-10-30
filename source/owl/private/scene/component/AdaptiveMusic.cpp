/**
 * @file AdaptiveMusic.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "data/assets/AssetReference.h"
#include "scene/component/AdaptiveMusic.h"

#include <algorithm>
#include <string>

namespace owl::scene::component {

void AdaptiveMusic::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;
	out << YAML::Key << "initialState" << YAML::Value << initialState;
	out << YAML::Key << "stateKey" << YAML::Value << stateKey;
	out << YAML::Key << "playOnStart" << YAML::Value << playOnStart;
	out << YAML::Key << "fadeSeconds" << YAML::Value << music.fadeSeconds;
	out << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
	for (const auto& layer: music.layers) {
		out << YAML::Flow << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << layer.name;
		out << YAML::Key << "asset" << YAML::Value << data::assets::toAssetReference(layer.asset);
		out << YAML::Key << "volume" << YAML::Value << layer.volume;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "states" << YAML::Value << YAML::BeginSeq;
	for (const auto& state: music.states) {
		out << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << state.name;
		out << YAML::Key << "layers" << YAML::Value << YAML::Flow << YAML::BeginSeq;
		for (const auto& layer: state.layers) out << layer;
		out << YAML::EndSeq;
		out << YAML::Key << "stinger" << YAML::Value << data::assets::toAssetReference(state.stinger);
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
}

void AdaptiveMusic::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	initialState = node["initialState"].as<std::string>("");
	stateKey = node["stateKey"].as<std::string>("");
	playOnStart = node["playOnStart"].as<bool>(true);
	music = {};
	music.fadeSeconds = std::max(0.f, node["fadeSeconds"].as<float>(2.f));
	if (const auto layers = node["layers"]; layers.isSequence()) {
		for (const auto layer: layers)
			music.layers.push_back({.name = layer["name"].as<std::string>(""),
									.asset = data::assets::toAssetPath(layer["asset"].as<std::string>("")),
									.volume = std::max(0.f, layer["volume"].as<float>(1.f))});
	}
	if (const auto states = node["states"]; states.isSequence()) {
		for (const auto state: states) {
			sound::MusicState read{.name = state["name"].as<std::string>(""),
								   .layers = {},
								   .stinger = data::assets::toAssetPath(state["stinger"].as<std::string>(""))};
			if (const auto stateLayers = state["layers"]; stateLayers.isSequence())
				for (const auto layer: stateLayers) read.layers.push_back(layer.as<std::string>(""));
			music.states.push_back(std::move(read));
		}
	}
	runtimePlayer.reset();
}

}// namespace owl::scene::component
