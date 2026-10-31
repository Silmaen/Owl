/**
 * @file Sequence.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/sequence/Sequence.h"

#include "core/FormatVersionYaml.h"
#include "core/Macros.h"
#include "core/external/yaml.h"
#include "data/assets/AssetSearchPaths.h"
#include "platform/AtomicFile.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <numbers>
#include <span>
#include <sstream>
#include <string>
#include <utility>

namespace owl::data::sequence {

namespace {

constexpr std::array<core::MigrationStep, 0> g_sequenceMigrations{};
constexpr core::DocumentFormat g_sequenceFormat{.name = "Sequence", .migrations = g_sequenceMigrations};

// Position, rotation, field of view and size of a key, interpolated channel by channel.
using Channels = std::array<float, 8>;

auto toChannels(const CameraKey& iKey) -> Channels {
	return {iKey.position.x(), iKey.position.y(), iKey.position.z(), iKey.rotation.x(),
			iKey.rotation.y(), iKey.rotation.z(), iKey.fov,          iKey.size};
}

auto fromChannels(const Channels& iChannels, const CameraKey& iModel) -> CameraKey {
	CameraKey key = iModel;
	key.position = {iChannels[0], iChannels[1], iChannels[2]};
	key.rotation = {iChannels[3], iChannels[4], iChannels[5]};
	key.fov = iChannels[6];
	key.size = iChannels[7];
	return key;
}

auto wrapAngle(const float iAngle) -> float {
	constexpr float twoPi = 2.f * std::numbers::pi_v<float>;
	return iAngle - twoPi * std::round(iAngle / twoPi);
}

// The angles of a key moved by whole turns to be the nearest to a reference, so a turn takes the short way.
auto unwrapNear(const Channels& iReference, Channels iChannels) -> Channels {
	for (size_t axis = 3; axis < 6; ++axis)
		iChannels[axis] = iReference[axis] + wrapAngle(iChannels[axis] - iReference[axis]);
	return iChannels;
}

auto hermite(const float iP0, const float iM0, const float iP1, const float iM1, const float iU, const float iSpan)
		-> float {
	const float u2 = iU * iU;
	const float u3 = u2 * iU;
	return (2.f * u3 - 3.f * u2 + 1.f) * iP0 + (u3 - 2.f * u2 + iU) * iSpan * iM0 + (-2.f * u3 + 3.f * u2) * iP1 +
		   (u3 - u2) * iSpan * iM1;
}

// Catmull-Rom tangent at a key from its neighbours, flat at the ends of the track.
auto tangent(const Channels* iPrevious, const Channels* iNext, const float iSpan, const size_t iChannel) -> float {
	if (iPrevious == nullptr || iNext == nullptr || iSpan <= 0.f)
		return 0.f;
	return ((*iNext)[iChannel] - (*iPrevious)[iChannel]) / iSpan;
}

auto smoothSegment(const std::vector<CameraKey>& iKeys, const size_t iIndex, const float iU) -> Channels {
	const Channels p0 = toChannels(iKeys[iIndex]);
	const Channels p1 = unwrapNear(p0, toChannels(iKeys[iIndex + 1]));
	const bool hasBefore = iIndex > 0;
	const bool hasAfter = iIndex + 2 < iKeys.size();
	const Channels before = hasBefore ? unwrapNear(p0, toChannels(iKeys[iIndex - 1])) : p0;
	const Channels after = hasAfter ? unwrapNear(p1, toChannels(iKeys[iIndex + 2])) : p1;
	const float span = iKeys[iIndex + 1].time - iKeys[iIndex].time;
	const float spanBefore = hasBefore ? iKeys[iIndex + 1].time - iKeys[iIndex - 1].time : 0.f;
	const float spanAfter = hasAfter ? iKeys[iIndex + 2].time - iKeys[iIndex].time : 0.f;
	Channels result{};
	for (size_t channel = 0; channel < result.size(); ++channel) {
		const float m0 = tangent(hasBefore ? &before : nullptr, &p1, spanBefore, channel);
		const float m1 = tangent(&p0, hasAfter ? &after : nullptr, spanAfter, channel);
		result[channel] = hermite(p0[channel], m0, p1[channel], m1, iU, span);
	}
	return result;
}

auto linearSegment(const CameraKey& iFrom, const CameraKey& iTo, const float iU) -> Channels {
	const Channels p0 = toChannels(iFrom);
	const Channels p1 = unwrapNear(p0, toChannels(iTo));
	Channels result{};
	for (size_t channel = 0; channel < result.size(); ++channel)
		result[channel] = p0[channel] + (p1[channel] - p0[channel]) * iU;
	return result;
}

void emitVec3(YAML::Emitter& ioOut, const char* iName, const math::vec3& iValue) {
	ioOut << YAML::Key << iName << YAML::Value << YAML::Flow << YAML::BeginSeq << iValue.x() << iValue.y() << iValue.z()
		  << YAML::EndSeq;
}

auto readVec3(const YAML::Node& iNode, const math::vec3& iDefault) -> math::vec3 {
	if (!iNode || !iNode.IsSequence() || iNode.size() != 3)
		return iDefault;
	return {iNode[0].as<float>(0.f), iNode[1].as<float>(0.f), iNode[2].as<float>(0.f)};
}

auto readCameraTrack(const YAML::Node& iNode) -> CameraTrack {
	CameraTrack track;
	track.name = iNode["name"].as<std::string>("");
	track.muted = iNode["muted"].as<bool>(false);
	const CameraKey model;
	for (const auto& item: iNode["keys"]) {
		CameraKey key;
		key.time = std::max(0.f, item["time"].as<float>(0.f));
		key.position = readVec3(item["position"], model.position);
		key.rotation = readVec3(item["rotation"], model.rotation);
		key.fov = item["fov"].as<float>(model.fov);
		key.size = item["size"].as<float>(model.size);
		key.easing = parseEasing(item["easing"].as<std::string>("")).value_or(Easing::Smooth);
		track.keys.push_back(key);
	}
	return track;
}

auto readEventTrack(const YAML::Node& iNode) -> EventTrack {
	EventTrack track;
	track.name = iNode["name"].as<std::string>("");
	track.muted = iNode["muted"].as<bool>(false);
	for (const auto& item: iNode["keys"])
		track.keys.push_back(
				{.time = std::max(0.f, item["time"].as<float>(0.f)), .function = item["function"].as<std::string>("")});
	return track;
}

}// namespace

auto getEasingName(const Easing iEasing) -> std::string_view { return magic_enum::enum_name(iEasing); }

auto parseEasing(const std::string_view iName) -> std::optional<Easing> { return magic_enum::enum_cast<Easing>(iName); }

auto getEasings() -> std::span<const Easing> {
	static constexpr auto s_easings = magic_enum::enum_values<Easing>();
	return s_easings;
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto CameraKey::operator==(const CameraKey& iOther) const -> bool = default;

auto CameraTrack::operator==(const CameraTrack& iOther) const -> bool = default;

auto EventKey::operator==(const EventKey& iOther) const -> bool = default;

auto EventTrack::operator==(const EventTrack& iOther) const -> bool = default;

auto Sequence::operator==(const Sequence& iOther) const -> bool = default;
OWL_DIAG_POP

auto CameraTrack::getStart() const -> float { return keys.empty() ? 0.f : keys.front().time; }

auto CameraTrack::getEnd() const -> float { return keys.empty() ? 0.f : keys.back().time; }

auto CameraTrack::sample(const float iTime) const -> std::optional<CameraKey> {
	if (keys.empty())
		return std::nullopt;
	if (iTime <= keys.front().time)
		return keys.front();
	if (iTime >= keys.back().time)
		return keys.back();
	const auto next = std::ranges::upper_bound(keys, iTime, {}, &CameraKey::time);
	const auto index = static_cast<size_t>(std::distance(keys.begin(), next)) - 1;
	const CameraKey& from = keys[index];
	const CameraKey& to = keys[index + 1];
	const float span = to.time - from.time;
	if (span <= 0.f)
		return to;
	const float u = (iTime - from.time) / span;
	CameraKey result;
	switch (from.easing) {
		case Easing::Step:
			return from;
		case Easing::Linear:
			result = fromChannels(linearSegment(from, to, u), from);
			break;
		case Easing::Smooth:
			result = fromChannels(smoothSegment(keys, index, u), from);
			break;
	}
	result.time = iTime;
	return result;
}

auto Sequence::isSequence(const std::filesystem::path& iPath) -> bool { return iPath.extension() == fileExtension(); }

auto Sequence::format() -> const core::DocumentFormat& { return g_sequenceFormat; }

auto Sequence::makeDefault() -> Sequence {
	Sequence sequence;
	CameraTrack shot{.name = "Shot 1", .keys = {}, .muted = false};
	shot.keys.push_back({.time = 0.f, .position = {0.f, 2.f, 10.f}});
	shot.keys.push_back({.time = sequence.duration, .position = {0.f, 2.f, 5.f}});
	sequence.cameraTracks.push_back(std::move(shot));
	sequence.eventTracks.push_back({.name = "Events", .keys = {}, .muted = false});
	return sequence;
}

auto Sequence::serializeToString() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Sequence" << YAML::Value << "";
	core::emitFormatVersion(out, g_sequenceFormat);
	out << YAML::Key << "duration" << YAML::Value << duration;
	out << YAML::Key << "cameraTracks" << YAML::Value << YAML::BeginSeq;
	for (const auto& track: cameraTracks) {
		out << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << track.name;
		if (track.muted)
			out << YAML::Key << "muted" << YAML::Value << true;
		out << YAML::Key << "keys" << YAML::Value << YAML::BeginSeq;
		for (const auto& key: track.keys) {
			out << YAML::Flow << YAML::BeginMap;
			out << YAML::Key << "time" << YAML::Value << key.time;
			emitVec3(out, "position", key.position);
			emitVec3(out, "rotation", key.rotation);
			out << YAML::Key << "fov" << YAML::Value << key.fov;
			out << YAML::Key << "size" << YAML::Value << key.size;
			out << YAML::Key << "easing" << YAML::Value << std::string{getEasingName(key.easing)};
			out << YAML::EndMap;
		}
		out << YAML::EndSeq << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "eventTracks" << YAML::Value << YAML::BeginSeq;
	for (const auto& track: eventTracks) {
		out << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << track.name;
		if (track.muted)
			out << YAML::Key << "muted" << YAML::Value << true;
		out << YAML::Key << "keys" << YAML::Value << YAML::BeginSeq;
		for (const auto& key: track.keys)
			out << YAML::Flow << YAML::BeginMap << YAML::Key << "time" << YAML::Value << key.time << YAML::Key
				<< "function" << YAML::Value << key.function << YAML::EndMap;
		out << YAML::EndSeq << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	return std::string{out.c_str()};
}

auto Sequence::deserializeFromString(const std::string_view iYaml) -> std::optional<Sequence> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("Sequence: Failed to parse YAML: {}.", e.what())
		return std::nullopt;
	}
	if (!root || !root.IsMap() || !root["Sequence"]) {
		OWL_CORE_WARN("Sequence: Document is not a sequence.")
		return std::nullopt;
	}
	if (!core::upgradeYamlDocument(g_sequenceFormat, root, "<buffer>")) {
		OWL_CORE_ERROR("Sequence: Document cannot be read.")
		return std::nullopt;
	}
	try {
		Sequence sequence;
		sequence.duration = std::max(0.f, root["duration"].as<float>(sequence.duration));
		for (const auto& item: root["cameraTracks"]) sequence.cameraTracks.push_back(readCameraTrack(item));
		for (const auto& item: root["eventTracks"]) sequence.eventTracks.push_back(readEventTrack(item));
		sequence.sortKeys();
		return sequence;
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("Sequence: Invalid value: {}.", e.what())
		return std::nullopt;
	}
}

auto Sequence::saveToFile(const std::filesystem::path& iPath) const -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString()); !written) {
		OWL_CORE_ERROR("Sequence: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto Sequence::loadFile(const std::filesystem::path& iPath) -> std::optional<Sequence> {
	const std::ifstream file(iPath);
	if (!file) {
		OWL_CORE_WARN("Sequence: Cannot open '{}'.", iPath.string())
		return std::nullopt;
	}
	std::stringstream text;
	text << file.rdbuf();
	return deserializeFromString(text.str());
}

auto Sequence::load(const std::filesystem::path& iAsset) -> std::optional<Sequence> {
	const auto bytes = data::assets::readAssetBytes(iAsset);
	if (!bytes) {
		OWL_CORE_WARN("Sequence: '{}' not found in the pack nor the asset folders.", iAsset.string())
		return std::nullopt;
	}
	return deserializeFromString({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
}

void Sequence::sortKeys() {
	for (auto& track: cameraTracks) std::ranges::stable_sort(track.keys, {}, &CameraKey::time);
	for (auto& track: eventTracks) std::ranges::stable_sort(track.keys, {}, &EventKey::time);
}

auto Sequence::sampleCamera(const float iTime) const -> std::optional<CameraShot> {
	std::optional<size_t> live;
	std::optional<size_t> lastEnded;
	std::optional<size_t> earliest;
	for (size_t index = 0; index < cameraTracks.size(); ++index) {
		const auto& track = cameraTracks[index];
		if (track.muted || track.keys.empty())
			continue;
		if (track.getStart() <= iTime && iTime <= track.getEnd())
			live = index;
		else if (track.getEnd() < iTime && (!lastEnded || cameraTracks[*lastEnded].getEnd() <= track.getEnd()))
			lastEnded = index;
		if (!earliest || track.getStart() < cameraTracks[*earliest].getStart())
			earliest = index;
	}
	auto chosen = live;
	if (!chosen)
		chosen = lastEnded ? lastEnded : earliest;
	if (!chosen)
		return std::nullopt;
	const auto pose = cameraTracks[*chosen].sample(iTime);
	if (!pose)
		return std::nullopt;
	return CameraShot{.pose = *pose, .track = *chosen};
}

auto Sequence::getEvents(const float iFrom, const float iTo, const bool iIncludeFrom) const
		-> std::vector<const EventKey*> {
	std::vector<const EventKey*> events;
	for (const auto& track: eventTracks) {
		if (track.muted)
			continue;
		for (const auto& key: track.keys) {
			const bool after = iIncludeFrom ? key.time >= iFrom : key.time > iFrom;
			if (after && key.time <= iTo && !key.function.empty())
				events.push_back(&key);
		}
	}
	std::ranges::stable_sort(events, {}, [](const EventKey* iKey) -> float { return iKey->time; });
	return events;
}

auto Sequence::getFunctions() const -> std::vector<std::string> {
	std::vector<std::string> names;
	for (const auto& track: eventTracks)
		for (const auto& key: track.keys)
			if (!key.function.empty())
				names.push_back(key.function);
	std::ranges::sort(names);
	const auto [first, last] = std::ranges::unique(names);
	names.erase(first, last);
	return names;
}

}// namespace owl::data::sequence
