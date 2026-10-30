/**
 * @file AudioEffects.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "math/vectors.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace owl::sound {

/**
 * @brief
 *  Direct filter of a sound: a gain for the whole sound, the low and the high frequencies.
 *
 * A low-pass filter lowers `gainHigh` (underwater, behind a wall), a high-pass one lowers `gainLow` (a radio), both
 * make a band-pass. Two filters combine by multiplying their gains.
 */
struct OWL_API Filter {
	/// Gain of the whole sound, in [0, 1].
	float gain{1.f};
	/// Gain of the low frequencies (below 250 Hz), in [0, 1]: lowered by a high-pass filter.
	float gainLow{1.f};
	/// Gain of the high frequencies (above 5 kHz), in [0, 1]: lowered by a low-pass filter.
	float gainHigh{1.f};

	/**
	 * @brief
	 *  Whether the filter changes nothing.
	 * @return True when every gain is 1.
	 */
	[[nodiscard]] auto isNeutral() const noexcept -> bool;

	/**
	 * @brief
	 *  The filter of this one followed by another.
	 * @param[in] iOther The other filter.
	 * @return The gains multiplied.
	 */
	[[nodiscard]] auto combined(const Filter& iOther) const noexcept -> Filter;

	/**
	 * @brief
	 *  The filter with its gains held in [0, 1].
	 * @return The clamped filter.
	 */
	[[nodiscard]] auto clamped() const noexcept -> Filter;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two filters.
	 * @param[in] iOther The other filter.
	 * @return True when every gain matches.
	 */
	auto operator==(const Filter& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  Parameters of the standard EFX reverb (`AL_EFFECT_REVERB`), as OpenAL Soft reads them.
 */
struct OWL_API Reverb {
	/// Modal density of the late reverberation, in [0, 1].
	float density{1.f};
	/// Echo density of the late reverberation, in [0, 1].
	float diffusion{1.f};
	/// Master gain of the reflected sound, in [0, 1].
	float gain{0.32f};
	/// High frequency attenuation of the reflected sound, in [0, 1].
	float gainHigh{0.89f};
	/// Reverberation decay time, in seconds [0.1, 20].
	float decayTime{1.49f};
	/// Ratio of the high frequency decay time to the low one, in [0.1, 2].
	float decayHighRatio{0.83f};
	/// Gain of the early reflections, in [0, 3.16].
	float reflectionsGain{0.05f};
	/// Delay of the early reflections, in seconds [0, 0.3].
	float reflectionsDelay{0.007f};
	/// Gain of the late reverberation, in [0, 10].
	float lateGain{1.26f};
	/// Delay of the late reverberation after the early reflections, in seconds [0, 0.1].
	float lateDelay{0.011f};

	/**
	 * @brief
	 *  The parameters held in the ranges EFX accepts.
	 * @return The clamped reverb.
	 */
	[[nodiscard]] auto clamped() const noexcept -> Reverb;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two reverbs.
	 * @param[in] iOther The other reverb.
	 * @return True when every parameter matches.
	 */
	auto operator==(const Reverb& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  Reverb presets of the EFX preset table (`efx-presets.h`), plus Custom for hand-set parameters.
 */
enum struct ReverbPreset : uint8_t {
	Custom,///< Parameters set by hand.
	Generic,///< A neutral medium room.
	Room,///< A small furnished room.
	StoneRoom,///< A bare stone room.
	Hallway,///< A long corridor.
	Hall,///< A concert hall.
	Arena,///< A large covered arena.
	Cave,///< A cave: long, bright decay.
	SewerPipe,///< A sewer pipe: strong, metallic.
	Forest,///< A forest: scattered, dull reflections.
	Plain,///< An open plain: almost dry.
	Mountains,///< Mountains: distant echoes.
	CityStreets,///< City streets.
	Underwater,///< Underwater: dark and resonant.
};

/**
 * @brief
 *  Parameters of a reverb preset.
 * @param[in] iPreset The preset (Custom gives the Generic values).
 * @return The reverb.
 */
[[nodiscard]] OWL_API auto getReverbPreset(ReverbPreset iPreset) -> Reverb;

/**
 * @brief
 *  Name of a reverb preset (`Cave`, `Underwater`...).
 * @param[in] iPreset The preset.
 * @return The name.
 */
[[nodiscard]] OWL_API auto getReverbPresetName(ReverbPreset iPreset) -> std::string_view;

/**
 * @brief
 *  Reverb preset of a name, as getReverbPresetName writes it.
 * @param[in] iName The name.
 * @return The preset, none for an unknown name.
 */
[[nodiscard]] OWL_API auto parseReverbPreset(std::string_view iName) -> std::optional<ReverbPreset>;

/**
 * @brief
 *  Interpolate two reverbs parameter by parameter.
 * @param[in] iFrom The reverb at 0.
 * @param[in] iTo The reverb at 1.
 * @param[in] iRatio The ratio, in [0, 1].
 * @return The interpolated reverb.
 */
[[nodiscard]] OWL_API auto mixReverb(const Reverb& iFrom, const Reverb& iTo, float iRatio) -> Reverb;

/**
 * @brief
 *  What one reverb zone gives at the listener's position.
 */
struct OWL_API AudioZoneSample {
	/// Reverb of the zone.
	Reverb reverb;
	/// Filter the zone puts on the sounds heard inside it.
	Filter filter;
	/// Weight of the zone at the listener, in [0, 1] (see getZoneWeight).
	float weight{0.f};
};

/**
 * @brief
 *  Environment the listener hears: the reverb blended from the zones around it, how much of it, and the zone filter.
 */
struct OWL_API AudioEnvironment {
	/// Blended reverb.
	Reverb reverb;
	/// Gain of the reverb send, in [0, 1]: 0 outside every zone.
	float wet{0.f};
	/// Filter of the zones, applied to the sounds of the environment buses.
	Filter filter;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two environments.
	 * @param[in] iOther The other environment.
	 * @return True when everything matches.
	 */
	auto operator==(const AudioEnvironment& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  Weight of a zone at the listener, from its distance outside the zone volume.
 *
 * 1 inside the volume, falling linearly to 0 at `iBlendDistance` outside it (a zone without blend distance has a hard
 * edge).
 * @param[in] iDistanceOutside Distance from the listener to the zone volume, 0 or negative inside.
 * @param[in] iBlendDistance Width of the fade around the volume, in world units.
 * @return The weight, in [0, 1].
 */
[[nodiscard]] OWL_API auto getZoneWeight(float iDistanceOutside, float iBlendDistance) -> float;

/**
 * @brief
 *  Blend the zones around the listener into the environment it hears.
 *
 * The reverb is the weighted average of the zones; the send gain is the total weight, at most 1, so the reverb fades
 * out across the blend distance of the last zone; the filter goes from neutral to the weighted average of the zone
 * filters the same way.
 * @param[in] iZones The zones with a weight above 0.
 * @return The environment.
 */
[[nodiscard]] OWL_API auto blendAudioZones(std::span<const AudioZoneSample> iZones) -> AudioEnvironment;

/// Highest speed given to the Doppler effect, in world units per second (the speed of sound).
constexpr float g_maxDopplerSpeed = 343.3f;

/**
 * @brief
 *  Velocity of a sound source or of the listener from two world positions a frame apart.
 *
 * World positions are in double, so a shift of the floating origin does not show as motion. The speed is held at
 * g_maxDopplerSpeed (a teleport would otherwise read as a supersonic jump).
 * @param[in] iPrevious World position at the previous frame.
 * @param[in] iCurrent World position at this frame.
 * @param[in] iSeconds Time between the two, in seconds.
 * @return The velocity, zero when no time went by.
 */
[[nodiscard]] OWL_API auto getDopplerVelocity(const math::vec3d& iPrevious, const math::vec3d& iCurrent,
											  double iSeconds) -> math::vec3f;

}// namespace owl::sound
