/**
 * @file WorldHash.h
 * @author Silmaen
 * @date 03/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>

namespace owl::scene {

class Scene;

/**
 * @brief
 *  Hash of the simulated state of a scene, to tell two runs of the same inputs apart.
 *
 * Covers the tick count, the seed and every random stream, the game state, and every entity in UUID order: all its
 * registered components as the scene file writes them (floats to the bit) and the velocities of its physics body.
 * Two runs of the same scene, seed and input log give the same hash; a desync shows as a different one. Rendering
 * state (caches, GPU resources) is left out.
 * @param[in] iScene The scene.
 * @return The 64-bit FNV-1a hash.
 */
[[nodiscard]] OWL_API auto computeWorldHash(const Scene& iScene) -> uint64_t;

}// namespace owl::scene
