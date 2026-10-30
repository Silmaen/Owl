/**
 * @file Mixer.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "AudioEffects.h"
#include "Bus.h"
#include "SoundAPI.h"
#include "SoundData.h"
#include "SoundHandle.h"
#include "core/Core.h"
#include "core/Macros.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace owl::sound {

/**
 * @brief
 *  Shape of a fade.
 */
enum struct FadeCurve : uint8_t {
	Linear,///< Gain changes at a constant rate.
	EqualPower,///< Sine / cosine: two tracks crossfading keep a constant loudness.
};

/**
 * @brief
 *  Gain of a fade at a point of its course.
 *
 * Linear: `from + (to - from) * t`. Equal power: `from * cos(t * pi / 2) + to * sin(t * pi / 2)`, so a fade out and
 * a fade in of the same length add up to a constant power.
 * @param[in] iFrom Gain at the start.
 * @param[in] iTo Gain at the end.
 * @param[in] iProgress Progress of the fade, in [0, 1].
 * @param[in] iCurve Shape of the fade.
 * @return The gain.
 */
[[nodiscard]] OWL_API auto getFadeGain(float iFrom, float iTo, float iProgress, FadeCurve iCurve) -> float;

/**
 * @brief
 *  Designer settings of one bus, saved with the scene.
 */
struct OWL_API BusSettings {
	/// Volume of the bus, in [0, 2].
	float volume{1.f};
	/// Whether the bus is silent.
	bool muted{false};
	/// Whether the bus is soloed: while one bus is, only the soloed ones are heard (Master ignores it).
	bool solo{false};

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two bus settings.
	 * @param[in] iOther The other settings.
	 * @return True when they match.
	 */
	auto operator==(const BusSettings& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  The mix of a scene: the settings of every bus and the Doppler factor.
 */
struct OWL_API MixSettings {
	/// Settings of each bus, by getBusIndex.
	std::array<BusSettings, g_busCount> buses{};
	/// Scale of the Doppler effect: 0 disables it, 1 is physical (343 units per second).
	float dopplerFactor{1.f};

	/**
	 * @brief
	 *  Settings of a bus.
	 * @param[in] iBus The bus.
	 * @return Its settings.
	 */
	[[nodiscard]] auto getBus(const Bus iBus) const -> const BusSettings& { return buses.at(getBusIndex(iBus)); }

	/**
	 * @brief
	 *  Settings of a bus (mutable).
	 * @param[in] iBus The bus.
	 * @return Its settings.
	 */
	[[nodiscard]] auto getBus(const Bus iBus) -> BusSettings& { return buses.at(getBusIndex(iBus)); }

	/**
	 * @brief
	 *  The settings with their values held in range.
	 * @return The clamped settings.
	 */
	[[nodiscard]] auto clamped() const -> MixSettings;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two mixes.
	 * @param[in] iOther The other mix.
	 * @return True when they match.
	 */
	auto operator==(const MixSettings& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  Mixer of the sound backend: routes every source to a bus, applies the bus gains, the fades, the filters and the
 *  environment of the reverb zones, then hands the result to the backend.
 *
 * The gain a source receives is `source volume * fade * bus gain`; the bus gain is `designer volume * user volume`
 * (the settings `volume_*`) of the bus and of Master, 0 when muted or when another bus is soloed.
 */
class OWL_API Mixer final {
public:
	/**
	 * @brief
	 *  Create a mixer over a backend.
	 * @param[in,out] ioApi The backend, which must outlive the mixer.
	 */
	explicit Mixer(SoundAPI& ioApi);

	Mixer(const Mixer&) = delete;

	Mixer(Mixer&&) = delete;

	auto operator=(const Mixer&) -> Mixer& = delete;

	auto operator=(Mixer&&) -> Mixer& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~Mixer();

	/**
	 * @brief
	 *  Set the designer volume of a bus.
	 * @param[in] iBus The bus.
	 * @param[in] iVolume The volume, held in [0, 2].
	 */
	void setBusVolume(Bus iBus, float iVolume);

	/**
	 * @brief
	 *  Designer volume of a bus.
	 * @param[in] iBus The bus.
	 * @return The volume.
	 */
	[[nodiscard]] auto getBusVolume(Bus iBus) const -> float;

	/**
	 * @brief
	 *  Set the player's volume of a bus (the `volume_*` settings).
	 * @param[in] iBus The bus.
	 * @param[in] iVolume The volume, held in [0, 2].
	 */
	void setUserVolume(Bus iBus, float iVolume);

	/**
	 * @brief
	 *  Player's volume of a bus.
	 * @param[in] iBus The bus.
	 * @return The volume.
	 */
	[[nodiscard]] auto getUserVolume(Bus iBus) const -> float;

	/**
	 * @brief
	 *  Mute or unmute a bus.
	 * @param[in] iBus The bus.
	 * @param[in] iMuted True to mute it.
	 */
	void setBusMuted(Bus iBus, bool iMuted);

	/**
	 * @brief
	 *  Whether a bus is muted.
	 * @param[in] iBus The bus.
	 * @return True when muted.
	 */
	[[nodiscard]] auto isBusMuted(Bus iBus) const -> bool;

	/**
	 * @brief
	 *  Solo a bus or end its solo.
	 * @param[in] iBus The bus (Master is ignored).
	 * @param[in] iSolo True to solo it.
	 */
	void setBusSolo(Bus iBus, bool iSolo);

	/**
	 * @brief
	 *  Whether a bus is soloed.
	 * @param[in] iBus The bus.
	 * @return True when soloed.
	 */
	[[nodiscard]] auto isBusSolo(Bus iBus) const -> bool;

	/**
	 * @brief
	 *  Effective gain of a bus: its volumes and Master's, mute and solo applied.
	 * @param[in] iBus The bus.
	 * @return The gain.
	 */
	[[nodiscard]] auto getBusGain(Bus iBus) const -> float;

	/**
	 * @brief
	 *  Replace the designer settings of every bus and the Doppler factor (a scene starting).
	 * @param[in] iMix The mix.
	 */
	void setMix(const MixSettings& iMix);

	/**
	 * @brief
	 *  Designer settings of every bus and the Doppler factor.
	 * @return The mix.
	 */
	[[nodiscard]] auto getMix() const -> MixSettings;

	/**
	 * @brief
	 *  Set the Doppler factor of the backend.
	 * @param[in] iFactor The factor, at least 0.
	 */
	void setDopplerFactor(float iFactor);

	/**
	 * @brief
	 *  Doppler factor.
	 * @return The factor.
	 */
	[[nodiscard]] auto getDopplerFactor() const -> float { return m_dopplerFactor; }

	/**
	 * @brief
	 *  Play a sound on its bus (SFX when the parameters name none).
	 * @param[in] iData The sound.
	 * @param[in] iParams The playback parameters; `volume` is the source volume, before the bus gain.
	 * @return The handle, invalid when the backend refused it.
	 */
	auto play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle;

	/**
	 * @brief
	 *  Stop a sound now.
	 * @param[in] iHandle The handle.
	 */
	void stop(SoundHandle iHandle);

	/**
	 * @brief
	 *  Set the source volume of a sound (before the fade and the bus gain).
	 * @param[in] iHandle The handle.
	 * @param[in] iVolume The volume.
	 */
	void setVolume(SoundHandle iHandle, float iVolume);

	/**
	 * @brief
	 *  Source volume of a sound.
	 * @param[in] iHandle The handle.
	 * @return The volume, none for an unknown handle.
	 */
	[[nodiscard]] auto getVolume(SoundHandle iHandle) const -> std::optional<float>;

	/**
	 * @brief
	 *  Bus of a sound.
	 * @param[in] iHandle The handle.
	 * @return The bus, none for an unknown handle.
	 */
	[[nodiscard]] auto getSourceBus(SoundHandle iHandle) const -> std::optional<Bus>;

	/**
	 * @brief
	 *  Gain handed to the backend for a sound: volume, fade and bus gain.
	 * @param[in] iHandle The handle.
	 * @return The gain, none for an unknown handle.
	 */
	[[nodiscard]] auto getSourceGain(SoundHandle iHandle) const -> std::optional<float>;

	/**
	 * @brief
	 *  Set the direct filter of a sound (combined with the zone filter on an environment bus).
	 * @param[in] iHandle The handle.
	 * @param[in] iFilter The filter.
	 */
	void setFilter(SoundHandle iHandle, const Filter& iFilter);

	/**
	 * @brief
	 *  Fade a sound to a gain.
	 * @param[in] iHandle The handle.
	 * @param[in] iTarget Gain at the end of the fade, in [0, 1].
	 * @param[in] iSeconds Length of the fade (0: at once).
	 * @param[in] iCurve Shape of the fade.
	 * @param[in] iStopAtEnd Whether the sound stops when the fade ends.
	 */
	void fadeTo(SoundHandle iHandle, float iTarget, float iSeconds, FadeCurve iCurve = FadeCurve::EqualPower,
				bool iStopAtEnd = false);

	/**
	 * @brief
	 *  Set the fade gain of a sound at once, ending any fade.
	 * @param[in] iHandle The handle.
	 * @param[in] iGain The gain, in [0, 1].
	 */
	void setFadeGain(SoundHandle iHandle, float iGain);

	/**
	 * @brief
	 *  Fade gain of a sound.
	 * @param[in] iHandle The handle.
	 * @return The gain, none for an unknown handle.
	 */
	[[nodiscard]] auto getFadeGain(SoundHandle iHandle) const -> std::optional<float>;

	/**
	 * @brief
	 *  Whether a sound is fading.
	 * @param[in] iHandle The handle.
	 * @return True during a fade.
	 */
	[[nodiscard]] auto isFading(SoundHandle iHandle) const -> bool;

	/**
	 * @brief
	 *  Crossfade two sounds: the first fades out and stops, the second fades in from silence.
	 * @param[in] iFrom The sound fading out (may be invalid).
	 * @param[in] iTo The sound fading in (may be invalid).
	 * @param[in] iSeconds Length of the crossfade.
	 * @param[in] iCurve Shape of the fades.
	 */
	void crossfade(SoundHandle iFrom, SoundHandle iTo, float iSeconds, FadeCurve iCurve = FadeCurve::EqualPower);

	/**
	 * @brief
	 *  Make a sound the music track, crossfading from the current one.
	 * @param[in] iTrack The new track, already playing.
	 * @param[in] iSeconds Length of the crossfade.
	 */
	void setMusic(SoundHandle iTrack, float iSeconds);

	/**
	 * @brief
	 *  Fade out and stop the music track.
	 * @param[in] iSeconds Length of the fade.
	 */
	void stopMusic(float iSeconds);

	/**
	 * @brief
	 *  The music track.
	 * @return Its handle, invalidSoundHandle when none plays.
	 */
	[[nodiscard]] auto getMusic() const -> SoundHandle { return m_music; }

	/**
	 * @brief
	 *  Set the environment of the listener (blended reverb zones), applied to the environment buses.
	 * @param[in] iEnvironment The environment.
	 */
	void setEnvironment(const AudioEnvironment& iEnvironment);

	/**
	 * @brief
	 *  Environment of the listener.
	 * @return The environment.
	 */
	[[nodiscard]] auto getEnvironment() const -> const AudioEnvironment& { return m_environment; }

	/**
	 * @brief
	 *  Advance the fades and forget the sounds the backend released.
	 * @param[in] iSeconds Time since the last update.
	 */
	void update(float iSeconds);

	/**
	 * @brief
	 *  Forget every sound (after the backend stopped them all).
	 */
	void clear();

	/**
	 * @brief
	 *  Number of sounds the mixer tracks.
	 * @return The count.
	 */
	[[nodiscard]] auto getSourceCount() const -> size_t { return m_sources.size(); }

private:
	/// A sound and its mixing state.
	struct Source {
		/// Bus of the sound.
		Bus bus{Bus::SFX};
		/// Source volume.
		float volume{1.f};
		/// Direct filter.
		Filter filter;
		/// Current fade gain.
		float fade{1.f};
		/// Fade gain at the start of the fade.
		float fadeFrom{1.f};
		/// Fade gain at the end of the fade.
		float fadeTarget{1.f};
		/// Time spent in the fade.
		float fadeElapsed{0.f};
		/// Length of the fade, 0 when not fading.
		float fadeDuration{0.f};
		/// Shape of the fade.
		FadeCurve curve{FadeCurve::EqualPower};
		/// Whether the sound stops when the fade ends.
		bool stopAtEnd{false};
	};

	/**
	 * @brief
	 *  Gain of a source with the current bus gains.
	 * @param[in] iSource The source.
	 * @return The gain.
	 */
	[[nodiscard]] auto computeGain(const Source& iSource) const -> float;

	/**
	 * @brief
	 *  Hand the gain of a source to the backend.
	 * @param[in] iHandle The handle.
	 * @param[in] iSource The source.
	 */
	void applyGain(SoundHandle iHandle, const Source& iSource) const;

	/**
	 * @brief
	 *  Hand the filter of a source, with the zone filter, to the backend.
	 * @param[in] iHandle The handle.
	 * @param[in] iSource The source.
	 */
	void applyFilter(SoundHandle iHandle, const Source& iSource) const;

	/**
	 * @brief
	 *  Hand the gain of every source to the backend.
	 */
	void applyAllGains() const;

	/// The backend (non-owning).
	SoundAPI* mp_api{nullptr};
	/// Designer settings of each bus.
	std::array<BusSettings, g_busCount> m_buses{};
	/// Player's volume of each bus.
	std::array<float, g_busCount> m_userVolumes{1.f, 1.f, 1.f, 1.f, 1.f};
	/// Doppler factor.
	float m_dopplerFactor{1.f};
	/// Sounds by handle.
	std::unordered_map<SoundHandle, Source> m_sources;
	/// The music track.
	SoundHandle m_music{invalidSoundHandle};
	/// Environment of the listener.
	AudioEnvironment m_environment;
};

}// namespace owl::sound
