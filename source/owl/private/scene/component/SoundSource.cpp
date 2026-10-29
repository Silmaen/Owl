/**
 * @file SoundSource.cpp
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "data/assets/AssetReference.h"
#include "scene/component/SoundSource.h"
#include "sound/AudioEffects.h"
#include "sound/Bus.h"

#include <string>

namespace owl::scene::component {

void SoundSource::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "soundAsset" << YAML::Value
							<< data::assets::toAssetReference(sound.soundAsset);
	iOut.getImpl()->emitter << YAML::Key << "category" << YAML::Value << std::string(sound::getBusName(sound.category));
	iOut.getImpl()->emitter << YAML::Key << "volume" << YAML::Value << sound.volume;
	iOut.getImpl()->emitter << YAML::Key << "pitch" << YAML::Value << sound.pitch;
	iOut.getImpl()->emitter << YAML::Key << "loop" << YAML::Value << sound.loop;
	iOut.getImpl()->emitter << YAML::Key << "spatial" << YAML::Value << sound.spatial;
	iOut.getImpl()->emitter << YAML::Key << "playOnStart" << YAML::Value << sound.playOnStart;
	iOut.getImpl()->emitter << YAML::Key << "maxDistance" << YAML::Value << sound.maxDistance;
	iOut.getImpl()->emitter << YAML::Key << "rolloff" << YAML::Value << sound.rolloff;
	iOut.getImpl()->emitter << YAML::Key << "doppler" << YAML::Value << sound.doppler;
	if (!sound.filter.isNeutral()) {
		iOut.getImpl()->emitter << YAML::Key << "filter" << YAML::Value << YAML::Flow << YAML::BeginMap;
		iOut.getImpl()->emitter << YAML::Key << "gain" << YAML::Value << sound.filter.gain;
		iOut.getImpl()->emitter << YAML::Key << "gainLow" << YAML::Value << sound.filter.gainLow;
		iOut.getImpl()->emitter << YAML::Key << "gainHigh" << YAML::Value << sound.filter.gainHigh;
		iOut.getImpl()->emitter << YAML::EndMap;
	}
	iOut.getImpl()->emitter << YAML::EndMap;
}

void SoundSource::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["soundAsset"])
		sound.soundAsset = data::assets::toAssetPath(iNode.getImpl()->node["soundAsset"].as<std::string>());
	if (iNode.getImpl()->node["category"])
		sound.category = sound::parseBus(iNode.getImpl()->node["category"].as<std::string>())
								 .value_or(SceneSound::Category::SFX);
	if (iNode.getImpl()->node["volume"])
		sound.volume = iNode.getImpl()->node["volume"].as<float>();
	if (iNode.getImpl()->node["pitch"])
		sound.pitch = iNode.getImpl()->node["pitch"].as<float>();
	if (iNode.getImpl()->node["loop"])
		sound.loop = iNode.getImpl()->node["loop"].as<bool>();
	if (iNode.getImpl()->node["spatial"])
		sound.spatial = iNode.getImpl()->node["spatial"].as<bool>();
	if (iNode.getImpl()->node["playOnStart"])
		sound.playOnStart = iNode.getImpl()->node["playOnStart"].as<bool>();
	if (iNode.getImpl()->node["maxDistance"])
		sound.maxDistance = iNode.getImpl()->node["maxDistance"].as<float>();
	if (iNode.getImpl()->node["rolloff"])
		sound.rolloff = iNode.getImpl()->node["rolloff"].as<float>();
	sound.doppler = iNode.getImpl()->node["doppler"].as<bool>(true);
	sound.filter = {};
	if (const auto filter = iNode.getImpl()->node["filter"]; filter.isMap())
		sound.filter = sound::Filter{.gain = filter["gain"].as<float>(1.f),
									 .gainLow = filter["gainLow"].as<float>(1.f),
									 .gainHigh = filter["gainHigh"].as<float>(1.f)}
							   .clamped();
	sound.runtimeHandle = sound::invalidSoundHandle;
	sound.runtimeHasLastPosition = false;
}

}// namespace owl::scene::component
