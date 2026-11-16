/**
 * @file Match.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Macros.h"
#include "core/SerializerImpl.h"
#include "core/YamlNode.h"
#include "network/Match.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace owl::network {

namespace {

auto finiteOr(const float iValue, const float iFallback) -> float { return std::isfinite(iValue) ? iValue : iFallback; }

}// namespace

auto HardwareProfile::meets(const HardwareProfile& iMinimum) const -> bool {
	if (iMinimum.cpuThreads > 0 && cpuThreads < iMinimum.cpuThreads)
		return false;
	return iMinimum.frameMs <= 0.f || (frameMs > 0.f && frameMs <= iMinimum.frameMs);
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto HardwareProfile::operator==(const HardwareProfile& iOther) const -> bool = default;

auto MatchRules::operator==(const MatchRules& iOther) const -> bool = default;
OWL_DIAG_POP

auto MatchRules::clamped() const -> MatchRules {
	MatchRules out = *this;
	const MatchRules defaults;
	out.maxPlayers = std::clamp(maxPlayers, 1U, 64U);
	out.minimum.cpuThreads = std::min(minimum.cpuThreads, 256U);
	out.minimum.frameMs = std::clamp(finiteOr(minimum.frameMs, 0.f), 0.f, 1000.f);
	out.inputDelay = std::clamp(inputDelay, 1U, 30U);
	out.hashInterval = std::clamp(hashInterval, 1U, 3600U);
	out.snapshotRate = std::clamp(finiteOr(snapshotRate, defaults.snapshotRate), 1.f, 120.f);
	out.interpolationDelay = std::clamp(finiteOr(interpolationDelay, defaults.interpolationDelay), 0.f, 1.f);
	out.snapshotBudget = std::clamp(snapshotBudget, 1U, 1024U);
	out.lagCompensationWindow = std::clamp(finiteOr(lagCompensationWindow, defaults.lagCompensationWindow), 0.f, 5.f);
	out.correctionThreshold = std::clamp(finiteOr(correctionThreshold, defaults.correctionThreshold), 0.f, 100.f);
	return out;
}

auto MatchRules::toYaml() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "model" << YAML::Value << std::string{magic_enum::enum_name(model)};
	out << YAML::Key << "maxPlayers" << YAML::Value << maxPlayers;
	out << YAML::Key << "crossplay" << YAML::Value << crossplay;
	out << YAML::Key << "minimumCpuThreads" << YAML::Value << minimum.cpuThreads;
	out << YAML::Key << "minimumFrameMs" << YAML::Value << minimum.frameMs;
	out << YAML::Key << "inputDelay" << YAML::Value << inputDelay;
	out << YAML::Key << "hashInterval" << YAML::Value << hashInterval;
	out << YAML::Key << "snapshotRate" << YAML::Value << snapshotRate;
	out << YAML::Key << "interpolationDelay" << YAML::Value << interpolationDelay;
	out << YAML::Key << "snapshotBudget" << YAML::Value << snapshotBudget;
	out << YAML::Key << "lagCompensationWindow" << YAML::Value << lagCompensationWindow;
	out << YAML::Key << "correctionThreshold" << YAML::Value << correctionThreshold;
	out << YAML::EndMap;
	return out.c_str();
}

auto MatchRules::fromYaml(const std::string_view iYaml) -> MatchRules {
	MatchRules rules;
	try {
		const core::YamlDocument document{iYaml, "network rules"};
		const auto root = document.getRoot();
		rules.model = magic_enum::enum_cast<NetModel>(root["model"].as<std::string>("")).value_or(rules.model);
		rules.maxPlayers = root["maxPlayers"].as<uint32_t>(rules.maxPlayers);
		rules.crossplay = root["crossplay"].as<bool>(rules.crossplay);
		rules.minimum.cpuThreads = root["minimumCpuThreads"].as<uint32_t>(rules.minimum.cpuThreads);
		rules.minimum.frameMs = root["minimumFrameMs"].as<float>(rules.minimum.frameMs);
		rules.inputDelay = root["inputDelay"].as<uint32_t>(rules.inputDelay);
		rules.hashInterval = root["hashInterval"].as<uint32_t>(rules.hashInterval);
		rules.snapshotRate = root["snapshotRate"].as<float>(rules.snapshotRate);
		rules.interpolationDelay = root["interpolationDelay"].as<float>(rules.interpolationDelay);
		rules.snapshotBudget = root["snapshotBudget"].as<uint32_t>(rules.snapshotBudget);
		rules.lagCompensationWindow = root["lagCompensationWindow"].as<float>(rules.lagCompensationWindow);
		rules.correctionThreshold = root["correctionThreshold"].as<float>(rules.correctionThreshold);
	} catch (const core::YamlError& iError) {
		OWL_CORE_WARN("MatchRules: Invalid network rules ignored ({}).", iError.what())
		return {};
	}
	return rules.clamped();
}

auto getVersionNumber(const std::string_view iVersion) -> uint32_t {
	uint32_t hash = 2166136261U;
	for (const char character: iVersion) {
		hash ^= static_cast<uint8_t>(character);
		hash *= 16777619U;
	}
	return hash;
}

}// namespace owl::network
