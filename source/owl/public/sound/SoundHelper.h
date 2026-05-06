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
};

}// namespace owl::sound
