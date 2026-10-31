/**
 * @file SequencePlayer.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "data/assets/AssetReference.h"
#include "scene/component/SequencePlayer.h"

#include <cstdint>
#include <string>

namespace owl::scene::component {

void SequencePlayer::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// SequencePlayer
	out << YAML::Key << "sequence" << YAML::Value << data::assets::toAssetReference(sequence);
	out << YAML::Key << "camera" << YAML::Value << static_cast<uint64_t>(camera);
	out << YAML::Key << "playOnStart" << YAML::Value << playOnStart;
	out << YAML::Key << "loop" << YAML::Value << loop;
	out << YAML::Key << "speed" << YAML::Value << speed;
	if (!onFinished.empty())
		out << YAML::Key << "onFinished" << YAML::Value << onFinished;
	out << YAML::EndMap;// SequencePlayer
}

void SequencePlayer::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	sequence = data::assets::toAssetPath(node["sequence"].as<std::string>(""));
	camera = core::UUID{node["camera"].as<uint64_t>(0)};
	playOnStart = node["playOnStart"].as<bool>(playOnStart);
	loop = node["loop"].as<bool>(loop);
	speed = node["speed"].as<float>(speed);
	onFinished = node["onFinished"].as<std::string>("");
	m_started = false;
	m_playing = false;
	m_time = 0.f;
	m_eventsDone = false;
	m_replacedCameras.clear();
	m_sequence.reset();
}

}// namespace owl::scene::component
