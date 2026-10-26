/**
 * @file FloatingOriginSettings.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::scene {

/**
 * @brief
 *  Per-scene floating origin settings, saved in the scene file under `FloatingOrigin:`.
 *
 * When enabled, the `owl.floating_origin` system moves the world origin onto the primary camera each time the camera
 * goes farther than `shiftRadius` from it, so that the float positions the renderer and the physics see stay small.
 */
struct OWL_API FloatingOriginSettings {
	/// Smallest accepted shift radius, in world units.
	static constexpr double minShiftRadius = 1.0;

	/// Move the world origin with the primary camera at runtime.
	bool enabled = false;
	/// Distance from the origin, in world units, beyond which the origin moves onto the camera.
	double shiftRadius = 2000.0;

	/**
	 * @brief
	 *  Copy of these settings with every field brought into its accepted range.
	 * @return The clamped settings.
	 */
	[[nodiscard]] auto clamped() const -> FloatingOriginSettings;

	/**
	 * @brief
	 *  Field-wise comparison.
	 * @param[in] iOther The settings to compare with.
	 * @return True when all fields are equal.
	 */
	auto operator==(const FloatingOriginSettings& iOther) const -> bool;
};

}// namespace owl::scene
