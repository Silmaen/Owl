/**
 * @file SoundCue.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/SoundCue.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "data/assets/AssetReference.h"
#include "data/assets/AssetSearchPaths.h"
#include "platform/AtomicFile.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <string>

namespace owl::sound {

namespace {

constexpr std::array<core::MigrationStep, 0> g_soundCueMigrations{};
constexpr core::DocumentFormat g_soundCueFormat{.name = "SoundCue", .migrations = g_soundCueMigrations};

auto splitMix(uint64_t& ioState) -> uint64_t {
	ioState += 0x9e3779b97f4a7c15ULL;
	uint64_t value = ioState;
	value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27u)) * 0x94d049bb133111ebULL;
	return value ^ (value >> 31u);
}

// A value in [-1, 1) from the top 24 bits.
auto toSigned(const uint64_t iValue) -> float {
	return static_cast<float>(iValue >> 40u) / static_cast<float>(1u << 23u) - 1.f;
}

auto parseVariant(const YAML::Node& iNode) -> SoundVariant {
	return {.file = data::assets::toAssetPath(iNode["file"].as<std::string>("")),
			.trimStart = std::max(0.f, iNode["trimStart"].as<float>(0.f)),
			.trimEnd = std::max(0.f, iNode["trimEnd"].as<float>(0.f)),
			.loopStart = std::max(0.f, iNode["loopStart"].as<float>(0.f)),
			.loopEnd = std::max(0.f, iNode["loopEnd"].as<float>(0.f)),
			.weight = std::max(0.f, iNode["weight"].as<float>(1.f))};
}

}// namespace

auto SoundCue::isCue(const std::filesystem::path& iPath) -> bool { return iPath.extension() == fileExtension(); }

auto SoundCue::format() -> const core::DocumentFormat& { return g_soundCueFormat; }

auto SoundCue::serializeToString() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "SoundCue" << YAML::Value << "";
	core::emitFormatVersion(out, g_soundCueFormat);
	out << YAML::Key << "volume" << YAML::Value << volume;
	out << YAML::Key << "volumeJitter" << YAML::Value << volumeJitter;
	out << YAML::Key << "pitch" << YAML::Value << pitch;
	out << YAML::Key << "pitchJitter" << YAML::Value << pitchJitter;
	out << YAML::Key << "loop" << YAML::Value << loop;
	out << YAML::Key << "variants" << YAML::Value << YAML::BeginSeq;
	for (const auto& variant: variants) {
		out << YAML::BeginMap;
		out << YAML::Key << "file" << YAML::Value << data::assets::toAssetReference(variant.file);
		out << YAML::Key << "trimStart" << YAML::Value << variant.trimStart;
		out << YAML::Key << "trimEnd" << YAML::Value << variant.trimEnd;
		out << YAML::Key << "loopStart" << YAML::Value << variant.loopStart;
		out << YAML::Key << "loopEnd" << YAML::Value << variant.loopEnd;
		out << YAML::Key << "weight" << YAML::Value << variant.weight;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	return std::string{out.c_str()};
}

auto SoundCue::deserializeFromString(const std::string_view iYaml) -> std::optional<SoundCue> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("SoundCue: Failed to parse YAML: {}.", e.what())
		return std::nullopt;
	}
	if (!root || !root.IsMap() || !root["SoundCue"]) {
		OWL_CORE_WARN("SoundCue: Document is not a sound cue.")
		return std::nullopt;
	}
	if (!core::upgradeYamlDocument(g_soundCueFormat, root, "<buffer>")) {
		OWL_CORE_ERROR("SoundCue: Document cannot be read.")
		return std::nullopt;
	}
	try {
		SoundCue cue{.variants = {},
					 .volume = std::max(0.f, root["volume"].as<float>(1.f)),
					 .volumeJitter = std::clamp(root["volumeJitter"].as<float>(0.f), 0.f, 1.f),
					 .pitch = std::max(0.01f, root["pitch"].as<float>(1.f)),
					 .pitchJitter = std::clamp(root["pitchJitter"].as<float>(0.f), 0.f, 0.9f),
					 .loop = root["loop"].as<bool>(false)};
		for (const auto& variant: root["variants"]) cue.variants.push_back(parseVariant(variant));
		return cue;
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("SoundCue: Invalid value: {}.", e.what())
		return std::nullopt;
	}
}

auto SoundCue::saveToFile(const std::filesystem::path& iPath) const -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString()); !written) {
		OWL_CORE_ERROR("SoundCue: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto SoundCue::load(const std::filesystem::path& iPath) -> std::optional<SoundCue> {
	const auto bytes = data::assets::readAssetBytes(iPath);
	if (!bytes) {
		OWL_CORE_WARN("SoundCue: '{}' not found in the pack nor the asset folders.", iPath.string())
		return std::nullopt;
	}
	return deserializeFromString({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
}

auto SoundCue::pick(const uint64_t iRandom) const -> std::optional<SoundCuePick> {
	float total = 0.f;
	for (const auto& variant: variants) total += variant.weight;
	if (total <= 0.f) {
		OWL_CORE_WARN("SoundCue: No take to play.")
		return std::nullopt;
	}
	uint64_t state = iRandom;
	const float target = (toSigned(splitMix(state)) + 1.f) * 0.5f * total;
	SoundCuePick result{.variant = 0,
						.volume = volume * (1.f + volumeJitter * toSigned(splitMix(state))),
						.pitch = pitch * (1.f + pitchJitter * toSigned(splitMix(state)))};
	float sum = 0.f;
	for (size_t i = 0; i < variants.size(); ++i) {
		if (variants[i].weight <= 0.f)
			continue;
		result.variant = i;
		sum += variants[i].weight;
		if (target < sum)
			break;
	}
	return result;
}

auto getVariantSpecification(const SoundVariant& iVariant) -> SoundData::Specification {
	return {.file = iVariant.file,
			.trimStart = iVariant.trimStart,
			.trimEnd = iVariant.trimEnd,
			.loopStart = iVariant.loopStart,
			.loopEnd = iVariant.loopEnd};
}

auto getVariantKey(const SoundVariant& iVariant) -> std::string {
	if (iVariant.trimStart == 0.f && iVariant.trimEnd == 0.f && iVariant.loopStart == 0.f && iVariant.loopEnd == 0.f)
		return iVariant.file;
	return std::format("{}#{}:{}:{}:{}", iVariant.file, iVariant.trimStart, iVariant.trimEnd, iVariant.loopStart,
					   iVariant.loopEnd);
}

}// namespace owl::sound
