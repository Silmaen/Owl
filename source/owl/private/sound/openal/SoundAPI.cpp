/**
 * @file SoundAPI.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "SoundAPI.h"

#include "core/external/openal.h"
#include <debug/Profiler.h>

#include <algorithm>
#include <optional>
#include <ranges>

namespace owl::sound::openal {

namespace internal {

struct OpenAlAPI {
	ALCdevice* device = nullptr;
	ALCcontext* context = nullptr;

	void reset() {
		device = nullptr;
		context = nullptr;
	}
};
}// namespace internal

namespace {
internal::OpenAlAPI g_Device;

// EFX entry points, loaded when the device has ALC_EXT_EFX; the effect and the slot carry the environment reverb.
struct EfxApi {
	LPALGENEFFECTS genEffects = nullptr;
	LPALDELETEEFFECTS deleteEffects = nullptr;
	LPALEFFECTI effecti = nullptr;
	LPALEFFECTF effectf = nullptr;
	LPALGENAUXILIARYEFFECTSLOTS genSlots = nullptr;
	LPALDELETEAUXILIARYEFFECTSLOTS deleteSlots = nullptr;
	LPALAUXILIARYEFFECTSLOTI sloti = nullptr;
	LPALAUXILIARYEFFECTSLOTF slotf = nullptr;
	LPALGENFILTERS genFilters = nullptr;
	LPALDELETEFILTERS deleteFilters = nullptr;
	LPALFILTERI filteri = nullptr;
	LPALFILTERF filterf = nullptr;
	ALuint effect = 0;
	ALuint slot = 0;
	bool available = false;
};
EfxApi g_Efx;

template<typename T>
auto loadProc(const char* iName) -> T {
	return reinterpret_cast<T>(alGetProcAddress(iName));
}

void initEfx() {
	g_Efx = EfxApi{};
	if (alcIsExtensionPresent(g_Device.device, "ALC_EXT_EFX") == ALC_FALSE) {
		OWL_CORE_WARN("SoundAPI(OpenAL): No EFX extension, reverb zones and filters are off.")
		return;
	}
	g_Efx.genEffects = loadProc<LPALGENEFFECTS>("alGenEffects");
	g_Efx.deleteEffects = loadProc<LPALDELETEEFFECTS>("alDeleteEffects");
	g_Efx.effecti = loadProc<LPALEFFECTI>("alEffecti");
	g_Efx.effectf = loadProc<LPALEFFECTF>("alEffectf");
	g_Efx.genSlots = loadProc<LPALGENAUXILIARYEFFECTSLOTS>("alGenAuxiliaryEffectSlots");
	g_Efx.deleteSlots = loadProc<LPALDELETEAUXILIARYEFFECTSLOTS>("alDeleteAuxiliaryEffectSlots");
	g_Efx.sloti = loadProc<LPALAUXILIARYEFFECTSLOTI>("alAuxiliaryEffectSloti");
	g_Efx.slotf = loadProc<LPALAUXILIARYEFFECTSLOTF>("alAuxiliaryEffectSlotf");
	g_Efx.genFilters = loadProc<LPALGENFILTERS>("alGenFilters");
	g_Efx.deleteFilters = loadProc<LPALDELETEFILTERS>("alDeleteFilters");
	g_Efx.filteri = loadProc<LPALFILTERI>("alFilteri");
	g_Efx.filterf = loadProc<LPALFILTERF>("alFilterf");
	if (g_Efx.genEffects == nullptr || g_Efx.deleteEffects == nullptr || g_Efx.effecti == nullptr ||
		g_Efx.effectf == nullptr || g_Efx.genSlots == nullptr || g_Efx.deleteSlots == nullptr ||
		g_Efx.sloti == nullptr || g_Efx.slotf == nullptr || g_Efx.genFilters == nullptr ||
		g_Efx.deleteFilters == nullptr || g_Efx.filteri == nullptr || g_Efx.filterf == nullptr) {
		OWL_CORE_WARN("SoundAPI(OpenAL): EFX entry points missing, reverb zones and filters are off.")
		return;
	}
	alGetError();
	g_Efx.genEffects(1, &g_Efx.effect);
	g_Efx.effecti(g_Efx.effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
	g_Efx.genSlots(1, &g_Efx.slot);
	g_Efx.slotf(g_Efx.slot, AL_EFFECTSLOT_GAIN, 0.f);
	g_Efx.sloti(g_Efx.slot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(g_Efx.effect));
	if (const ALenum err = alGetError(); err != AL_NO_ERROR) {
		OWL_CORE_WARN("SoundAPI(OpenAL): EFX reverb could not be created ({}), reverb zones are off.", alGetString(err))
		return;
	}
	g_Efx.available = true;
}

void releaseEfx() {
	if (g_Efx.available) {
		g_Efx.deleteSlots(1, &g_Efx.slot);
		g_Efx.deleteEffects(1, &g_Efx.effect);
	}
	g_Efx = EfxApi{};
}

}// namespace

void SoundAPI::init() {
	OWL_PROFILE_FUNCTION()

	if (g_Device.device != nullptr) {
		OWL_CORE_WARN("SoundAPI(OpenAL): Device already initialized.")
	} else {
		g_Device.device = alcOpenDevice(nullptr);
		if (g_Device.device == nullptr) {
			OWL_CORE_ERROR("SoundAPI(OpenAL): Device could not be opened.")
			setState(State::Error);
			return;
		}
	}

	// Context
	if (g_Device.context != nullptr) {
		OWL_CORE_WARN("SoundAPI(OpenAL): Context already initialized.")
	} else {
		g_Device.context = alcCreateContext(g_Device.device, nullptr);
		if (g_Device.context == nullptr) {
			OWL_CORE_ERROR("SoundAPI(OpenAL): Context could not be created.")
			setState(State::Error);
			return;
		}
		if (alcMakeContextCurrent(g_Device.context) == 0) {
			OWL_CORE_ERROR("SoundAPI(OpenAL): Failed to make context current.")
			setState(State::Error);
			return;
		}
	}

	// Listener (initial position at origin)
	{
		constexpr std::array<ALfloat, 6> listenerOri = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
		alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
		alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
		alListenerfv(AL_ORIENTATION, listenerOri.data());
	}

	// Distance model for 3D attenuation
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
	alSpeedOfSound(g_maxDopplerSpeed);
	initEfx();

	setState(State::Ready);
}

SoundAPI::~SoundAPI() {
	OWL_PROFILE_FUNCTION()

	stopAll();
	releaseEfx();

	if (g_Device.context != nullptr) {
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(g_Device.context);
	}
	if (g_Device.device != nullptr) {
		alcCloseDevice(g_Device.device);
	}
	g_Device.reset();
	setState(State::Created);
}

void SoundAPI::playSound(const shared<SoundData>& iData) { play(iData, PlayParams{}); }

auto SoundAPI::play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle {
	OWL_PROFILE_FUNCTION()

	if (iData == nullptr) {
		OWL_CORE_WARN("SoundAPI(OpenAL)::play: SoundData is null.")
		return invalidSoundHandle;
	}
	const auto bufferId = static_cast<uint32_t>(iData->getSystemId());
	uint32_t source = 0;
	// Drop an error left by an earlier call, so it is not read as the one of alGenSources.
	alGetError();
	alGenSources(1, &source);
	ALenum err = alGetError();
	if (err == AL_OUT_OF_MEMORY && reclaimSource()) {
		alGenSources(1, &source);
		err = alGetError();
	}
	if (err != AL_NO_ERROR) {
		OWL_CORE_ERROR("SoundAPI(OpenAL)::play: alGenSources error: {} ({} sources playing).", alGetString(err),
					   m_handleToSource.size())
		return invalidSoundHandle;
	}

	alSourcei(source, AL_BUFFER, static_cast<ALint>(bufferId));
	alSourcef(source, AL_GAIN, iParams.volume);
	alSourcef(source, AL_PITCH, iParams.pitch);
	alSourcei(source, AL_LOOPING, iParams.loop ? AL_TRUE : AL_FALSE);

	if (iParams.spatial) {
		alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
		alSource3f(source, AL_POSITION, iParams.position.x(), iParams.position.y(), iParams.position.z());
		alSource3f(source, AL_VELOCITY, iParams.velocity.x(), iParams.velocity.y(), iParams.velocity.z());
		alSourcef(source, AL_MAX_DISTANCE, iParams.maxDistance);
		alSourcef(source, AL_ROLLOFF_FACTOR, iParams.rolloff);
		alSourcef(source, AL_REFERENCE_DISTANCE, 1.0f);
	} else {
		alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	}

	const SoundHandle handle = m_nextHandle++;
	m_handleToSource[handle] = source;
	if (!iParams.filter.isNeutral())
		setFilter(handle, iParams.filter);
	alSourcePlay(source);
	return handle;
}

void SoundAPI::stop(const SoundHandle iHandle) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end()) {
		alSourceStop(it->second);
		alDeleteSources(1, &it->second);
		m_handleToSource.erase(it);
	}
	releaseFilter(iHandle);
}

void SoundAPI::pause(const SoundHandle iHandle) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end())
		alSourcePause(it->second);
}

void SoundAPI::resume(const SoundHandle iHandle) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end())
		alSourcePlay(it->second);
}

void SoundAPI::setVolume(const SoundHandle iHandle, const float iVolume) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end())
		alSourcef(it->second, AL_GAIN, iVolume);
}

void SoundAPI::setPitch(const SoundHandle iHandle, const float iPitch) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end())
		alSourcef(it->second, AL_PITCH, iPitch);
}

void SoundAPI::setLoop(const SoundHandle iHandle, const bool iLoop) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end())
		alSourcei(it->second, AL_LOOPING, iLoop ? AL_TRUE : AL_FALSE);
}

void SoundAPI::setPosition(const SoundHandle iHandle, const math::vec3f& iPosition) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end())
		alSource3f(it->second, AL_POSITION, iPosition.x(), iPosition.y(), iPosition.z());
}

void SoundAPI::setVelocity(const SoundHandle iHandle, const math::vec3f& iVelocity) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end())
		alSource3f(it->second, AL_VELOCITY, iVelocity.x(), iVelocity.y(), iVelocity.z());
}

void SoundAPI::setListenerPosition(const math::vec3f& iPosition) {
	alListener3f(AL_POSITION, iPosition.x(), iPosition.y(), iPosition.z());
}

void SoundAPI::setListenerOrientation(const math::vec3f& iForward, const math::vec3f& iUp) {
	const std::array<ALfloat, 6> ori = {iForward.x(), iForward.y(), iForward.z(), iUp.x(), iUp.y(), iUp.z()};
	alListenerfv(AL_ORIENTATION, ori.data());
}

void SoundAPI::setListenerGain(const float iGain) { alListenerf(AL_GAIN, iGain); }

void SoundAPI::stopAll() {
	for (auto& source: m_handleToSource | std::views::values) {
		alSourceStop(source);
		alDeleteSources(1, &source);
	}
	m_handleToSource.clear();
	for (auto& filter: m_handleToFilter | std::views::values) g_Efx.deleteFilters(1, &filter);
	m_handleToFilter.clear();
}

void SoundAPI::releaseFilter(const SoundHandle iHandle) {
	if (const auto it = m_handleToFilter.find(iHandle); it != m_handleToFilter.end()) {
		g_Efx.deleteFilters(1, &it->second);
		m_handleToFilter.erase(it);
	}
}

void SoundAPI::setFilter(const SoundHandle iHandle, const Filter& iFilter) {
	const auto it = m_handleToSource.find(iHandle);
	if (it == m_handleToSource.end() || !g_Efx.available)
		return;
	if (iFilter.isNeutral()) {
		alSourcei(it->second, AL_DIRECT_FILTER, AL_FILTER_NULL);
		releaseFilter(iHandle);
		return;
	}
	auto [found, inserted] = m_handleToFilter.try_emplace(iHandle, 0U);
	if (inserted)
		g_Efx.genFilters(1, &found->second);
	const ALuint filter = found->second;
	const Filter clamped = iFilter.clamped();
	const bool lowOnly = clamped.gainLow >= 1.f;
	const bool highOnly = clamped.gainHigh >= 1.f;
	if (lowOnly) {
		g_Efx.filteri(filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
		g_Efx.filterf(filter, AL_LOWPASS_GAIN, clamped.gain);
		g_Efx.filterf(filter, AL_LOWPASS_GAINHF, clamped.gainHigh);
	} else if (highOnly) {
		g_Efx.filteri(filter, AL_FILTER_TYPE, AL_FILTER_HIGHPASS);
		g_Efx.filterf(filter, AL_HIGHPASS_GAIN, clamped.gain);
		g_Efx.filterf(filter, AL_HIGHPASS_GAINLF, clamped.gainLow);
	} else {
		g_Efx.filteri(filter, AL_FILTER_TYPE, AL_FILTER_BANDPASS);
		g_Efx.filterf(filter, AL_BANDPASS_GAIN, clamped.gain);
		g_Efx.filterf(filter, AL_BANDPASS_GAINLF, clamped.gainLow);
		g_Efx.filterf(filter, AL_BANDPASS_GAINHF, clamped.gainHigh);
	}
	alSourcei(it->second, AL_DIRECT_FILTER, static_cast<ALint>(filter));
}

void SoundAPI::setEnvironmentSend(const SoundHandle iHandle, const bool iSend) {
	const auto it = m_handleToSource.find(iHandle);
	if (it == m_handleToSource.end() || !g_Efx.available)
		return;
	alSource3i(it->second, AL_AUXILIARY_SEND_FILTER,
			   iSend ? static_cast<ALint>(g_Efx.slot) : static_cast<ALint>(AL_EFFECTSLOT_NULL), 0,
			   static_cast<ALint>(AL_FILTER_NULL));
}

void SoundAPI::setReverb(const Reverb& iReverb, const float iWet) {
	if (!g_Efx.available)
		return;
	const Reverb reverb = iReverb.clamped();
	const ALuint effect = g_Efx.effect;
	g_Efx.effectf(effect, AL_REVERB_DENSITY, reverb.density);
	g_Efx.effectf(effect, AL_REVERB_DIFFUSION, reverb.diffusion);
	g_Efx.effectf(effect, AL_REVERB_GAIN, reverb.gain);
	g_Efx.effectf(effect, AL_REVERB_GAINHF, reverb.gainHigh);
	g_Efx.effectf(effect, AL_REVERB_DECAY_TIME, reverb.decayTime);
	g_Efx.effectf(effect, AL_REVERB_DECAY_HFRATIO, reverb.decayHighRatio);
	g_Efx.effectf(effect, AL_REVERB_REFLECTIONS_GAIN, reverb.reflectionsGain);
	g_Efx.effectf(effect, AL_REVERB_REFLECTIONS_DELAY, reverb.reflectionsDelay);
	g_Efx.effectf(effect, AL_REVERB_LATE_REVERB_GAIN, reverb.lateGain);
	g_Efx.effectf(effect, AL_REVERB_LATE_REVERB_DELAY, reverb.lateDelay);
	// The slot copies the effect when it is attached: attach it again after each change.
	g_Efx.sloti(g_Efx.slot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(effect));
	g_Efx.slotf(g_Efx.slot, AL_EFFECTSLOT_GAIN, std::clamp(iWet, 0.f, 1.f));
}

void SoundAPI::setListenerVelocity(const math::vec3f& iVelocity) {
	alListener3f(AL_VELOCITY, iVelocity.x(), iVelocity.y(), iVelocity.z());
}

void SoundAPI::setDopplerFactor(const float iFactor) { alDopplerFactor(std::max(0.f, iFactor)); }

auto SoundAPI::hasSource(const SoundHandle iHandle) const -> bool { return m_handleToSource.contains(iHandle); }

auto SoundAPI::isPlaying(const SoundHandle iHandle) const -> bool {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end()) {
		ALint state = 0;
		alGetSourcei(it->second, AL_SOURCE_STATE, &state);
		return state == AL_PLAYING;
	}
	return false;
}

void SoundAPI::frame(const core::Timestep&) {
	OWL_PROFILE_FUNCTION()

	releaseStoppedSources();
}

void SoundAPI::releaseStoppedSources() {
	std::erase_if(m_handleToSource, [this](const auto& iEntry) -> bool {
		ALint state = 0;
		alGetSourcei(iEntry.second, AL_SOURCE_STATE, &state);
		if (state != AL_STOPPED)
			return false;
		alDeleteSources(1, &iEntry.second);
		releaseFilter(iEntry.first);
		return true;
	});
}

auto SoundAPI::reclaimSource() -> bool {
	const size_t before = m_handleToSource.size();
	releaseStoppedSources();
	if (m_handleToSource.size() < before)
		return true;
	// Every source plays: cut the oldest one-shot sound, the least missed.
	std::optional<SoundHandle> oldest;
	for (const auto& [handle, source]: m_handleToSource) {
		ALint looping = AL_FALSE;
		alGetSourcei(source, AL_LOOPING, &looping);
		if (looping == AL_FALSE && (!oldest.has_value() || handle < *oldest))
			oldest = handle;
	}
	if (!oldest.has_value())
		return false;
	OWL_CORE_WARN("SoundAPI(OpenAL)::play: No source left, the oldest sound is cut.")
	stop(*oldest);
	return true;
}

}// namespace owl::sound::openal
