/**
 * @file LightFlicker.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene::component {

/**
 * @brief
 *  Makes the light of an entity flicker: a fire, a candle or a pulsing lamp.
 *
 * The `owl.light_flicker` system multiplies the intensity of the `Light` of the same entity by a factor that depends
 * only on the seed and the scene time, shifts its colour (or the tint of a sprite) and jitters its position. The
 * authored values of the `Light`, the `Transform` and the sprite are the centre of the movement: they are read when
 * Play starts and never changed in the scene. `follow` makes an entity (the flame cards of a fire) breathe with the
 * flicker of another one.
 */
struct OWL_API LightFlicker {
	/// Shape of the flicker.
	enum struct Style : uint8_t {
		Fire,///< Layered noise: a slow swell, quick licks and a fast crackle.
		Candle,///< A wavering with frequencies drawn from the seed, so no two candles move together.
		Pulse,///< Two sines of unrelated rates (a torch or a lamp).
	};

	/// Shape of the flicker.
	Style style = Style::Fire;
	/// Whether the flicker runs; off leaves the light at its authored values.
	bool enabled = true;
	/// Largest relative swing of the intensity (0.28: from 72 % to 128 % of the authored intensity).
	float amplitude = 0.28f;
	/// Time scale of the flicker (2 flickers twice as fast).
	float speed = 1.f;
	/// Offset in the noise: two lights with different seeds never flicker together.
	float seed = 0.f;
	/// Added to the colour (sRGB) at the brightest moment and taken off at the dimmest: brighter burns yellower.
	math::vec3 colorShift{0.f, 0.f, 0.f};
	/// Largest displacement of the position along each axis, in world units.
	math::vec3 positionJitter{0.f, 0.f, 0.f};
	/// Dips per second (a draught bends the flame); 0 for none.
	float dipRate = 0.f;
	/// Share of the intensity a full dip takes off.
	float dipDepth = 0.5f;
	/// Entity whose flicker this one follows: its scale breathes with the intensity of that entity (0: none).
	core::UUID follow{0};
	/// Relative change of the scale along each axis for a relative change of 1 of the followed intensity.
	math::vec3 scaleResponse{0.f, 0.f, 0.f};

	/// Runtime: the authored values were read.
	bool m_captured = false;
	/// Runtime: authored intensity.
	float m_baseIntensity = 0.f;
	/// Runtime: authored colour.
	math::vec4 m_baseColor{1.f, 1.f, 1.f, 1.f};
	/// Runtime: authored position.
	math::vec3 m_basePosition{0.f, 0.f, 0.f};
	/// Runtime: authored scale.
	math::vec3 m_baseScale{1.f, 1.f, 1.f};
	/// Runtime: relative intensity change of the last frame (the factor minus one).
	float m_deviation = 0.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Light Flicker"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "LightFlicker"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
