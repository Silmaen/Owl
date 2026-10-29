/**
 * @file SoundHelper.h
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "SoundHandle.h"
#include "core/Core.h"

#include <string>

namespace owl::scene {

class Entity;
}

namespace owl::sound {
/**
 * @brief
 *  Helper utilities for playing sounds from gameplay code.
 */
class OWL_API SoundHelper final {
public:
	/**
	 * @brief
	 *  Play the sound attached to an entity's SoundSource component.
	 * @param[in,out] iEntity The entity with a SoundSource component.
	 * @return The sound handle, or invalidSoundHandle on failure.
	 */
	static auto playEntitySound(const scene::Entity& iEntity) -> SoundHandle;

	/**
	 * @brief
	 *  Stop the sound attached to an entity's SoundSource component.
	 * @param[in,out] iEntity The entity with a SoundSource component.
	 */
	static void stopEntitySound(const scene::Entity& iEntity);

	/**
	 * @brief
	 *  Play a sound asset: an audio file, or a `.owlsound` cue whose take, volume and pitch are picked at random.
	 *
	 * The sound data is loaded into the sound library on first use. A cue multiplies the volume and the pitch of the
	 * parameters and loops when either asks for it; when the parameters name no bus, the cue's bus is used.
	 * @param[in] iAsset The asset, relative to the asset folders.
	 * @param[in] iParams The play parameters.
	 * @return The sound handle, or invalidSoundHandle on failure.
	 */
	static auto playAsset(const std::string& iAsset, const PlayParams& iParams) -> SoundHandle;

	/**
	 * @brief
	 *  Drop a cue from the cache, so that its next play reads the file again.
	 * @param[in] iAsset The cue, relative to the asset folders.
	 */
	static void forgetCue(const std::string& iAsset);

	/**
	 * @brief
	 *  Play a music track on the Music bus, looping, crossfading from the track playing.
	 * @param[in] iAsset The asset, relative to the asset folders.
	 * @param[in] iFadeSeconds Length of the crossfade.
	 * @return The handle of the new track, or invalidSoundHandle on failure (the old track keeps playing).
	 */
	static auto playMusic(const std::string& iAsset, float iFadeSeconds) -> SoundHandle;
};

}// namespace owl::sound
