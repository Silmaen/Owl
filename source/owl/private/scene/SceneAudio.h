/**
 * @file SceneAudio.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"
#include "scene/Scene.h"
#include "sound/Mixer.h"

namespace owl::scene {

/**
 * @brief
 *  Start the adaptive musics of a scene that play on start.
 * @param[in,out] ioScene The scene starting.
 * @param[in,out] ioMixer The mixer.
 */
OWL_API void startAdaptiveMusic(Scene& ioScene, sound::Mixer& ioMixer);

/**
 * @brief
 *  One frame of the scene's audio (the `owl.sound` system): listener and sources placed with their Doppler
 *  velocities, the reverb zones blended at the listener, the adaptive musics following their game state key.
 * @param[in,out] ioScene The scene.
 * @param[in] iSeconds Time since the last frame.
 */
OWL_API void updateSceneAudio(Scene& ioScene, float iSeconds);

}// namespace owl::scene
