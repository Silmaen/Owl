/**
 * @file PhysicsSnapshot.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"

/**
 * @brief
 *  Scene management.
 */
namespace owl::scene {

/**
 * @brief
 *  Snapshot of a physics body's runtime state, saved with a game (`SaveManager`) and restored by the physics.
 */
struct PhysicsSnapshot {
	/// Linear velocity (Z stays 0 for a 2D body).
	math::vec3f linearVelocity{0.f, 0.f, 0.f};
	/// Angular velocity, radians per second around each axis (Z alone for a 2D body).
	math::vec3f angularVelocity{0.f, 0.f, 0.f};
	/// Whether the body is awake.
	bool awake = true;
};

}// namespace owl::scene
