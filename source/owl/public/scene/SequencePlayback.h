/**
 * @file SequencePlayback.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/sequence/Sequence.h"
#include "math/matrices.h"

#include <cstddef>
#include <optional>

namespace owl::scene {

class Entity;
class Scene;

/**
 * @brief
 *  A camera pose of a sequence in world space.
 */
struct OWL_API SequenceCameraPose {
	/// World transform of the camera.
	math::mat4 world;
	/// Vertical field of view of a perspective camera, in radians.
	float fov = 0.f;
	/// Size of an orthographic camera.
	float size = 0.f;
	/// Index of the camera track giving the pose.
	size_t track = 0;
};

/**
 * @brief
 *  Start the sequence of an entity's `SequencePlayer`, its camera made primary.
 * @param[in] iEntity The entity holding the player.
 * @param[in] iFrom Time to start from, in seconds.
 * @return False when the entity has no player or its sequence cannot be read.
 */
OWL_API auto playSequence(const Entity& iEntity, float iFrom = 0.f) -> bool;

/**
 * @brief
 *  Stop the sequence of an entity's `SequencePlayer`: the replaced cameras are primary again and its `onFinished`
 *  function is called.
 * @param[in] iEntity The entity holding the player.
 * @return False when the entity has no player playing.
 */
OWL_API auto stopSequence(const Entity& iEntity) -> bool;

/**
 * @brief
 *  Move the playback of an entity's `SequencePlayer` to a time, the camera placed at once; no event is called.
 * @param[in] iEntity The entity holding the player.
 * @param[in] iTime Time in seconds, clamped to the sequence.
 * @return False when the entity has no player playing.
 */
OWL_API auto seekSequence(const Entity& iEntity, float iTime) -> bool;

/**
 * @brief
 *  The world pose a sequence gives the camera at a time, for a player entity.
 * @param[in] iEntity The entity holding the player (the keys are in its space, in its parent's when it drives itself).
 * @param[in] iSequence The sequence.
 * @param[in] iTime Time in seconds.
 * @return The pose, nothing when no camera track has a key.
 */
OWL_API auto getSequenceCameraPose(const Entity& iEntity, const data::sequence::Sequence& iSequence, float iTime)
		-> std::optional<SequenceCameraPose>;

/**
 * @brief
 *  Advance every playing `SequencePlayer` of a scene, starting those that play on start.
 * @param[in,out] ioScene The scene.
 * @param[in] iSeconds Duration of the frame.
 */
OWL_API void updateSequencePlayers(Scene& ioScene, float iSeconds);

}// namespace owl::scene
