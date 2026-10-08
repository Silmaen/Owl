/**
 * @file PhysicsSettings.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "scene/PhysicsSettings.h"

/**
 * @brief
 *  Namespace for physics management.
 */
namespace owl::physics {

/// The per-scene physics settings live with the scene (`scene::PhysicsSettings`): the scene stores them.
using PhysicsSettings = scene::PhysicsSettings;

}// namespace owl::physics
