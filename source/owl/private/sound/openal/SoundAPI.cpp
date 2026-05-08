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

	setState(State::Ready);
}

SoundAPI::~SoundAPI() {
	OWL_PROFILE_FUNCTION()

	// Stop and delete all active sources
	for (auto& source: m_handleToSource | std::views::values) {
		alSourceStop(source);
		alDeleteSources(1, &source);
	}
	m_handleToSource.clear();

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
	alGenSources(1, &source);
	if (const ALenum err = alGetError(); err != AL_NO_ERROR) {
		OWL_CORE_ERROR("SoundAPI(OpenAL)::play: alGenSources error: {}.", alGetString(err))
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

	alSourcePlay(source);

	const SoundHandle handle = m_nextHandle++;
	m_handleToSource[handle] = source;
	return handle;
}

void SoundAPI::stop(const SoundHandle iHandle) {
	if (const auto it = m_handleToSource.find(iHandle); it != m_handleToSource.end()) {
		alSourceStop(it->second);
		alDeleteSources(1, &it->second);
		m_handleToSource.erase(it);
	}
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
	for (auto& [handle, source]: m_handleToSource) {
		alSourceStop(source);
		alDeleteSources(1, &source);
	}
	m_handleToSource.clear();
}

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

	std::vector<SoundHandle> toRemove;
	for (const auto& [handle, source]: m_handleToSource) {
		ALint state = 0;
		alGetSourcei(source, AL_SOURCE_STATE, &state);
		if (state == AL_STOPPED)
			toRemove.push_back(handle);
	}
	for (const auto handle: toRemove) {
		if (const auto it = m_handleToSource.find(handle); it != m_handleToSource.end()) {
			alDeleteSources(1, &it->second);
			m_handleToSource.erase(it);
		}
	}
}


}// namespace owl::sound::openal
