/**
 * @file Accessibility.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/Accessibility.h"
#include "scene/SettingsManager.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

namespace owl::scene {

namespace {

auto readNumber(const SettingsManager& iSettings, const char* iKey) -> std::optional<float> {
	if (const auto value = iSettings.getAs<float>(iKey); value.has_value())
		return value;
	if (const auto value = iSettings.getAs<int64_t>(iKey); value.has_value())
		return static_cast<float>(*value);
	return std::nullopt;
}

}// namespace

auto Accessibility::read(const SettingsManager& iSettings) -> Accessibility {
	Accessibility result;
	if (const auto mode = iSettings.getAs<std::string>(KeyColorBlindMode); mode.has_value())
		result.colorBlindMode = renderer::colorDeficiencyFromName(*mode).value_or(renderer::ColorDeficiency::None);
	if (const auto strength = readNumber(iSettings, KeyColorBlindStrength); strength.has_value())
		result.colorBlindStrength = std::clamp(*strength, 0.f, 1.f);
	if (const auto reduced = iSettings.getAs<bool>(KeyReducedMotion); reduced.has_value())
		result.reducedMotion = *reduced;
	if (const auto subtitles = iSettings.getAs<bool>(KeySubtitles); subtitles.has_value())
		result.subtitles = *subtitles;
	if (const auto scale = readNumber(iSettings, KeySubtitleScale); scale.has_value())
		result.subtitleScale = std::clamp(*scale, 0.5f, 3.f);
	if (const auto background = readNumber(iSettings, KeySubtitleBackground); background.has_value())
		result.subtitleBackground = std::clamp(*background, 0.f, 1.f);
	return result;
}

void Accessibility::setDefaults(SettingsManager& ioSettings) {
	const Accessibility defaults;
	ioSettings.setDefault(KeyColorBlindMode, std::string{renderer::toName(defaults.colorBlindMode)});
	ioSettings.setDefault(KeyColorBlindStrength, defaults.colorBlindStrength);
	ioSettings.setDefault(KeyReducedMotion, defaults.reducedMotion);
	ioSettings.setDefault(KeySubtitles, defaults.subtitles);
	ioSettings.setDefault(KeySubtitleScale, defaults.subtitleScale);
	ioSettings.setDefault(KeySubtitleBackground, defaults.subtitleBackground);
}

}// namespace owl::scene
