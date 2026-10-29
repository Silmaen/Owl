/**
 * @file AudioEffects.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/AudioEffects.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace owl::sound {

auto Filter::isNeutral() const noexcept -> bool { return gain >= 1.f && gainLow >= 1.f && gainHigh >= 1.f; }

auto Filter::combined(const Filter& iOther) const noexcept -> Filter {
	return {.gain = gain * iOther.gain, .gainLow = gainLow * iOther.gainLow, .gainHigh = gainHigh * iOther.gainHigh};
}

auto Filter::clamped() const noexcept -> Filter {
	return {.gain = std::clamp(gain, 0.f, 1.f),
			.gainLow = std::clamp(gainLow, 0.f, 1.f),
			.gainHigh = std::clamp(gainHigh, 0.f, 1.f)};
}

auto Reverb::clamped() const noexcept -> Reverb {
	return {.density = std::clamp(density, 0.f, 1.f),
			.diffusion = std::clamp(diffusion, 0.f, 1.f),
			.gain = std::clamp(gain, 0.f, 1.f),
			.gainHigh = std::clamp(gainHigh, 0.f, 1.f),
			.decayTime = std::clamp(decayTime, 0.1f, 20.f),
			.decayHighRatio = std::clamp(decayHighRatio, 0.1f, 2.f),
			.reflectionsGain = std::clamp(reflectionsGain, 0.f, 3.16f),
			.reflectionsDelay = std::clamp(reflectionsDelay, 0.f, 0.3f),
			.lateGain = std::clamp(lateGain, 0.f, 10.f),
			.lateDelay = std::clamp(lateDelay, 0.f, 0.1f)};
}

namespace {

// Values of EFX_REVERB_PRESET_* (efx-presets.h), the standard reverb subset.
constexpr auto makeReverb(const float iDensity, const float iDiffusion, const float iGainHigh, const float iDecay,
						  const float iDecayHigh, const float iReflections, const float iReflectionsDelay,
						  const float iLate, const float iLateDelay) -> Reverb {
	return {.density = iDensity,
			.diffusion = iDiffusion,
			.gain = 0.3162f,
			.gainHigh = iGainHigh,
			.decayTime = iDecay,
			.decayHighRatio = iDecayHigh,
			.reflectionsGain = iReflections,
			.reflectionsDelay = iReflectionsDelay,
			.lateGain = iLate,
			.lateDelay = iLateDelay};
}

}// namespace

auto getReverbPreset(const ReverbPreset iPreset) -> Reverb {
	switch (iPreset) {
		case ReverbPreset::Custom:
		case ReverbPreset::Generic:
			return makeReverb(1.f, 1.f, 0.8913f, 1.49f, 0.83f, 0.05f, 0.007f, 1.2589f, 0.011f);
		case ReverbPreset::Room:
			return makeReverb(0.4287f, 1.f, 0.5929f, 0.4f, 0.83f, 0.1503f, 0.002f, 1.0629f, 0.003f);
		case ReverbPreset::StoneRoom:
			return makeReverb(1.f, 1.f, 0.7079f, 2.31f, 0.64f, 0.4411f, 0.012f, 1.1003f, 0.017f);
		case ReverbPreset::Hallway:
			return makeReverb(0.3645f, 1.f, 0.7079f, 1.49f, 0.59f, 0.2458f, 0.007f, 1.6615f, 0.011f);
		case ReverbPreset::Hall:
			return makeReverb(1.f, 1.f, 0.5623f, 3.92f, 0.7f, 0.2427f, 0.02f, 0.9977f, 0.029f);
		case ReverbPreset::Arena:
			return makeReverb(1.f, 1.f, 0.4477f, 7.24f, 0.33f, 0.2612f, 0.02f, 1.0186f, 0.03f);
		case ReverbPreset::Cave:
			return makeReverb(1.f, 1.f, 1.f, 2.91f, 1.3f, 0.5f, 0.015f, 0.7063f, 0.022f);
		case ReverbPreset::SewerPipe:
			return makeReverb(0.3071f, 0.8f, 0.3162f, 2.81f, 0.14f, 1.6387f, 0.014f, 3.2471f, 0.021f);
		case ReverbPreset::Forest:
			return makeReverb(1.f, 0.3f, 0.0224f, 1.49f, 0.54f, 0.0525f, 0.162f, 0.7682f, 0.088f);
		case ReverbPreset::Plain:
			return makeReverb(1.f, 0.21f, 0.1f, 1.49f, 0.5f, 0.0585f, 0.179f, 0.1089f, 0.1f);
		case ReverbPreset::Mountains:
			return makeReverb(1.f, 0.27f, 0.0562f, 1.49f, 0.21f, 0.0407f, 0.3f, 0.1919f, 0.1f);
		case ReverbPreset::CityStreets:
			return makeReverb(1.f, 0.78f, 0.7079f, 1.79f, 1.12f, 0.2818f, 0.046f, 0.1995f, 0.028f);
		case ReverbPreset::Underwater:
			return makeReverb(0.3645f, 1.f, 0.01f, 1.49f, 0.1f, 0.5963f, 0.007f, 7.0795f, 0.011f);
	}
	return {};
}

auto getReverbPresetName(const ReverbPreset iPreset) -> std::string_view {
	switch (iPreset) {
		case ReverbPreset::Custom:
			return "Custom";
		case ReverbPreset::Generic:
			return "Generic";
		case ReverbPreset::Room:
			return "Room";
		case ReverbPreset::StoneRoom:
			return "StoneRoom";
		case ReverbPreset::Hallway:
			return "Hallway";
		case ReverbPreset::Hall:
			return "Hall";
		case ReverbPreset::Arena:
			return "Arena";
		case ReverbPreset::Cave:
			return "Cave";
		case ReverbPreset::SewerPipe:
			return "SewerPipe";
		case ReverbPreset::Forest:
			return "Forest";
		case ReverbPreset::Plain:
			return "Plain";
		case ReverbPreset::Mountains:
			return "Mountains";
		case ReverbPreset::CityStreets:
			return "CityStreets";
		case ReverbPreset::Underwater:
			return "Underwater";
	}
	return "Custom";
}

auto parseReverbPreset(const std::string_view iName) -> std::optional<ReverbPreset> {
	for (uint8_t index = 0; index <= static_cast<uint8_t>(ReverbPreset::Underwater); ++index) {
		const auto preset = static_cast<ReverbPreset>(index);
		if (getReverbPresetName(preset) == iName)
			return preset;
	}
	return std::nullopt;
}

auto mixReverb(const Reverb& iFrom, const Reverb& iTo, const float iRatio) -> Reverb {
	const float ratio = std::clamp(iRatio, 0.f, 1.f);
	const auto mix = [ratio](const float iA, const float iB) -> float { return iA + (iB - iA) * ratio; };
	return {.density = mix(iFrom.density, iTo.density),
			.diffusion = mix(iFrom.diffusion, iTo.diffusion),
			.gain = mix(iFrom.gain, iTo.gain),
			.gainHigh = mix(iFrom.gainHigh, iTo.gainHigh),
			.decayTime = mix(iFrom.decayTime, iTo.decayTime),
			.decayHighRatio = mix(iFrom.decayHighRatio, iTo.decayHighRatio),
			.reflectionsGain = mix(iFrom.reflectionsGain, iTo.reflectionsGain),
			.reflectionsDelay = mix(iFrom.reflectionsDelay, iTo.reflectionsDelay),
			.lateGain = mix(iFrom.lateGain, iTo.lateGain),
			.lateDelay = mix(iFrom.lateDelay, iTo.lateDelay)};
}

auto getZoneWeight(const float iDistanceOutside, const float iBlendDistance) -> float {
	if (iDistanceOutside <= 0.f)
		return 1.f;
	if (iBlendDistance <= 0.f)
		return 0.f;
	return std::clamp(1.f - iDistanceOutside / iBlendDistance, 0.f, 1.f);
}

auto blendAudioZones(const std::span<const AudioZoneSample> iZones) -> AudioEnvironment {
	float total = 0.f;
	for (const auto& zone: iZones) total += std::max(0.f, zone.weight);
	if (total <= 0.f)
		return {};
	// Running weighted average: each zone pulls the result toward its values by its share of the weight so far.
	Reverb reverb;
	Filter filter;
	float seen = 0.f;
	for (const auto& zone: iZones) {
		const float weight = std::max(0.f, zone.weight);
		if (weight <= 0.f)
			continue;
		const bool first = seen <= 0.f;
		seen += weight;
		if (first) {
			reverb = zone.reverb;
			filter = zone.filter;
			continue;
		}
		const float share = weight / seen;
		reverb = mixReverb(reverb, zone.reverb, share);
		filter = {.gain = filter.gain + (zone.filter.gain - filter.gain) * share,
				  .gainLow = filter.gainLow + (zone.filter.gainLow - filter.gainLow) * share,
				  .gainHigh = filter.gainHigh + (zone.filter.gainHigh - filter.gainHigh) * share};
	}
	const float wet = std::min(total, 1.f);
	const Filter neutral;
	return {.reverb = reverb,
			.wet = wet,
			.filter = {.gain = neutral.gain + (filter.gain - neutral.gain) * wet,
					   .gainLow = neutral.gainLow + (filter.gainLow - neutral.gainLow) * wet,
					   .gainHigh = neutral.gainHigh + (filter.gainHigh - neutral.gainHigh) * wet}};
}

auto getDopplerVelocity(const math::vec3d& iPrevious, const math::vec3d& iCurrent, const double iSeconds)
		-> math::vec3f {
	if (iSeconds <= 0.0)
		return {0.f, 0.f, 0.f};
	math::vec3d velocity = (iCurrent - iPrevious) / iSeconds;
	if (const double speed = velocity.norm(); speed > static_cast<double>(g_maxDopplerSpeed))
		velocity *= static_cast<double>(g_maxDopplerSpeed) / speed;
	return {static_cast<float>(velocity.x()), static_cast<float>(velocity.y()), static_cast<float>(velocity.z())};
}

}// namespace owl::sound
