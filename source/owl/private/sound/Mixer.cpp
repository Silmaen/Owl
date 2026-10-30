/**
 * @file Mixer.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/Mixer.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace owl::sound {

namespace {

constexpr float g_maxBusVolume = 2.f;

auto clampVolume(const float iVolume) -> float {
	return std::isfinite(iVolume) ? std::clamp(iVolume, 0.f, g_maxBusVolume) : 1.f;
}

}// namespace

auto getFadeGain(const float iFrom, const float iTo, const float iProgress, const FadeCurve iCurve) -> float {
	const float progress = std::clamp(iProgress, 0.f, 1.f);
	switch (iCurve) {
		case FadeCurve::Linear:
			return iFrom + (iTo - iFrom) * progress;
		case FadeCurve::EqualPower:
			{
				const float angle = progress * std::numbers::pi_v<float> * 0.5f;
				return iFrom * std::cos(angle) + iTo * std::sin(angle);
			}
	}
	return iTo;
}

auto MixSettings::clamped() const -> MixSettings {
	MixSettings result = *this;
	for (auto& bus: result.buses) bus.volume = clampVolume(bus.volume);
	result.getBus(Bus::Master).solo = false;
	result.dopplerFactor = std::isfinite(dopplerFactor) ? std::clamp(dopplerFactor, 0.f, 10.f) : 1.f;
	return result;
}

Mixer::Mixer(SoundAPI& ioApi) : mp_api{&ioApi} {}

Mixer::~Mixer() = default;

void Mixer::setBusVolume(const Bus iBus, const float iVolume) {
	m_buses.at(getBusIndex(iBus)).volume = clampVolume(iVolume);
	applyAllGains();
}

auto Mixer::getBusVolume(const Bus iBus) const -> float { return m_buses.at(getBusIndex(iBus)).volume; }

void Mixer::setUserVolume(const Bus iBus, const float iVolume) {
	m_userVolumes.at(getBusIndex(iBus)) = clampVolume(iVolume);
	applyAllGains();
}

auto Mixer::getUserVolume(const Bus iBus) const -> float { return m_userVolumes.at(getBusIndex(iBus)); }

void Mixer::setBusMuted(const Bus iBus, const bool iMuted) {
	m_buses.at(getBusIndex(iBus)).muted = iMuted;
	applyAllGains();
}

auto Mixer::isBusMuted(const Bus iBus) const -> bool { return m_buses.at(getBusIndex(iBus)).muted; }

void Mixer::setBusSolo(const Bus iBus, const bool iSolo) {
	if (iBus == Bus::Master)
		return;
	m_buses.at(getBusIndex(iBus)).solo = iSolo;
	applyAllGains();
}

auto Mixer::isBusSolo(const Bus iBus) const -> bool { return m_buses.at(getBusIndex(iBus)).solo; }

auto Mixer::getBusGain(const Bus iBus) const -> float {
	const auto& master = m_buses.at(getBusIndex(Bus::Master));
	const float masterGain = master.muted ? 0.f : master.volume * m_userVolumes.at(getBusIndex(Bus::Master));
	if (iBus == Bus::Master)
		return masterGain;
	const auto& bus = m_buses.at(getBusIndex(iBus));
	if (bus.muted)
		return 0.f;
	const bool anySolo =
			std::ranges::any_of(m_buses, [](const BusSettings& iSettings) -> bool { return iSettings.solo; });
	if (anySolo && !bus.solo)
		return 0.f;
	return bus.volume * m_userVolumes.at(getBusIndex(iBus)) * masterGain;
}

void Mixer::setMix(const MixSettings& iMix) {
	const MixSettings mix = iMix.clamped();
	m_buses = mix.buses;
	setDopplerFactor(mix.dopplerFactor);
	applyAllGains();
}

auto Mixer::getMix() const -> MixSettings { return {.buses = m_buses, .dopplerFactor = m_dopplerFactor}; }

void Mixer::setDopplerFactor(const float iFactor) {
	m_dopplerFactor = std::isfinite(iFactor) ? std::max(0.f, iFactor) : 1.f;
	mp_api->setDopplerFactor(m_dopplerFactor);
}

auto Mixer::play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle {
	Source source{.bus = iParams.bus.value_or(Bus::SFX), .volume = iParams.volume, .filter = iParams.filter};
	PlayParams params = iParams;
	params.volume = computeGain(source);
	if (isEnvironmentBus(source.bus))
		params.filter = source.filter.combined(m_environment.filter);
	const SoundHandle handle = mp_api->play(iData, params);
	if (handle == invalidSoundHandle)
		return handle;
	mp_api->setEnvironmentSend(handle, isEnvironmentBus(source.bus));
	m_sources.insert_or_assign(handle, source);
	return handle;
}

void Mixer::stop(const SoundHandle iHandle) {
	mp_api->stop(iHandle);
	m_sources.erase(iHandle);
	if (iHandle == m_music)
		m_music = invalidSoundHandle;
}

void Mixer::setVolume(const SoundHandle iHandle, const float iVolume) {
	const auto it = m_sources.find(iHandle);
	if (it == m_sources.end()) {
		mp_api->setVolume(iHandle, iVolume);
		return;
	}
	it->second.volume = iVolume;
	applyGain(iHandle, it->second);
}

auto Mixer::getVolume(const SoundHandle iHandle) const -> std::optional<float> {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		return it->second.volume;
	return std::nullopt;
}

auto Mixer::getSourceBus(const SoundHandle iHandle) const -> std::optional<Bus> {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		return it->second.bus;
	return std::nullopt;
}

auto Mixer::getSourceGain(const SoundHandle iHandle) const -> std::optional<float> {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		return computeGain(it->second);
	return std::nullopt;
}

void Mixer::setFilter(const SoundHandle iHandle, const Filter& iFilter) {
	const auto it = m_sources.find(iHandle);
	if (it == m_sources.end())
		return;
	it->second.filter = iFilter;
	applyFilter(iHandle, it->second);
}

void Mixer::fadeTo(const SoundHandle iHandle, const float iTarget, const float iSeconds, const FadeCurve iCurve,
				   const bool iStopAtEnd) {
	const auto it = m_sources.find(iHandle);
	if (it == m_sources.end())
		return;
	auto& source = it->second;
	source.fadeFrom = source.fade;
	source.fadeTarget = std::clamp(iTarget, 0.f, 1.f);
	source.fadeElapsed = 0.f;
	source.fadeDuration = std::max(0.f, iSeconds);
	source.curve = iCurve;
	source.stopAtEnd = iStopAtEnd;
	if (source.fadeDuration <= 0.f) {
		source.fade = source.fadeTarget;
		if (iStopAtEnd) {
			stop(iHandle);
			return;
		}
	}
	applyGain(iHandle, source);
}

void Mixer::setFadeGain(const SoundHandle iHandle, const float iGain) {
	const auto it = m_sources.find(iHandle);
	if (it == m_sources.end())
		return;
	auto& source = it->second;
	source.fade = std::clamp(iGain, 0.f, 1.f);
	source.fadeFrom = source.fade;
	source.fadeTarget = source.fade;
	source.fadeDuration = 0.f;
	source.stopAtEnd = false;
	applyGain(iHandle, source);
}

auto Mixer::getFadeGain(const SoundHandle iHandle) const -> std::optional<float> {
	if (const auto it = m_sources.find(iHandle); it != m_sources.end())
		return it->second.fade;
	return std::nullopt;
}

auto Mixer::isFading(const SoundHandle iHandle) const -> bool {
	const auto it = m_sources.find(iHandle);
	return it != m_sources.end() && it->second.fadeDuration > 0.f;
}

void Mixer::crossfade(const SoundHandle iFrom, const SoundHandle iTo, const float iSeconds, const FadeCurve iCurve) {
	if (iFrom == iTo)
		return;
	if (iTo != invalidSoundHandle && iSeconds > 0.f) {
		setFadeGain(iTo, 0.f);
		fadeTo(iTo, 1.f, iSeconds, iCurve);
	}
	if (iFrom != invalidSoundHandle)
		fadeTo(iFrom, 0.f, iSeconds, iCurve, true);
}

void Mixer::setMusic(const SoundHandle iTrack, const float iSeconds) {
	const SoundHandle previous = m_music;
	m_music = iTrack;
	crossfade(previous, iTrack, iSeconds);
}

void Mixer::stopMusic(const float iSeconds) {
	const SoundHandle previous = m_music;
	m_music = invalidSoundHandle;
	if (previous != invalidSoundHandle)
		fadeTo(previous, 0.f, iSeconds, FadeCurve::EqualPower, true);
}

void Mixer::setEnvironment(const AudioEnvironment& iEnvironment) {
	if (iEnvironment == m_environment)
		return;
	const bool filterChanged = iEnvironment.filter != m_environment.filter;
	m_environment = iEnvironment;
	mp_api->setReverb(m_environment.reverb, m_environment.wet);
	if (!filterChanged)
		return;
	for (const auto& [handle, source]: m_sources)
		if (isEnvironmentBus(source.bus))
			applyFilter(handle, source);
}

void Mixer::update(const float iSeconds) {
	std::erase_if(m_sources, [this](const auto& iEntry) -> bool { return !mp_api->hasSource(iEntry.first); });
	if (m_music != invalidSoundHandle && !m_sources.contains(m_music))
		m_music = invalidSoundHandle;
	std::vector<SoundHandle> finished;
	for (auto& [handle, source]: m_sources) {
		if (source.fadeDuration <= 0.f)
			continue;
		source.fadeElapsed += std::max(0.f, iSeconds);
		const float progress = std::min(1.f, source.fadeElapsed / source.fadeDuration);
		source.fade = sound::getFadeGain(source.fadeFrom, source.fadeTarget, progress, source.curve);
		if (progress >= 1.f) {
			source.fade = source.fadeTarget;
			source.fadeDuration = 0.f;
			if (source.stopAtEnd)
				finished.push_back(handle);
		}
		applyGain(handle, source);
	}
	for (const SoundHandle handle: finished) stop(handle);
}

void Mixer::clear() {
	m_sources.clear();
	m_music = invalidSoundHandle;
}

auto Mixer::computeGain(const Source& iSource) const -> float {
	return iSource.volume * iSource.fade * getBusGain(iSource.bus);
}

void Mixer::applyGain(const SoundHandle iHandle, const Source& iSource) const {
	mp_api->setVolume(iHandle, computeGain(iSource));
}

void Mixer::applyFilter(const SoundHandle iHandle, const Source& iSource) const {
	mp_api->setFilter(iHandle,
					  isEnvironmentBus(iSource.bus) ? iSource.filter.combined(m_environment.filter) : iSource.filter);
}

void Mixer::applyAllGains() const {
	for (const auto& [handle, source]: m_sources) applyGain(handle, source);
}

}// namespace owl::sound
