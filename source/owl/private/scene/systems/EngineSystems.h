/**
 * @file EngineSystems.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "scene/SystemSchedule.h"

/**
 * @brief
 *  The runtime systems the engine schedules by default (`SystemSchedule::getDefault()`).
 */
namespace owl::scene::systems {

/**
 * @brief
 *  Add the engine systems to a schedule, in their phases and run order.
 * @param[in,out] ioSchedule The schedule to fill.
 */
void registerEngineSystems(SystemSchedule& ioSchedule);

/**
 * @brief
 *  Put the physics bodies' simulated poses back into their transforms before gameplay (`owl.physics_poses`).
 *
 * The transforms hold the interpolated (rendered) poses after the physics phase: a script reading one and writing it
 * back to the body would rewind the body by the interpolation lag on every frame.
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void restorePhysicsPoses(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Run `NativeScript` and `LuaScript` updates of the visible entities (`owl.scripts`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateScripts(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Move the `FlyCamera` entities from the keyboard and mouse (`owl.fly_cameras`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateFlyCameras(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Drive `VoxelPlayer` entities: look, modes, block edits, gravity, jump and collision (`owl.voxel_players`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateVoxelPlayers(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Advance `RaycastDoor` / `RaycastPushWall` state machines for one tick (`owl.raycast_walls`).
 *
 * Handles the engine-built-in activation path (player proximity + key edge), advances the open/close (or
 * one-shot slide) animation, updates each entity's local transform along `slideDirection`, and mirrors that to
 * the kinematic Box2D body. Lua scripts that bypass the built-in path (by setting `interactionKey` to `0`) drive
 * the same state machine through `door.activate` / `pushwall.activate`.
 * @param[in,out] ioScene The scene.
 * @param[in] iTimeStep The elapsed time in seconds for this tick.
 */
OWL_API void updateRaycastDynamicWalls(Scene& ioScene, float iTimeStep);

/**
 * @brief
 *  Read the inputs of the primary `Player` (`owl.player_input`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updatePlayerInput(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Step the scene's physics world, then dispatch the begun collisions to the scripts (`owl.physics`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updatePhysics(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Move every `EntityLink` host onto its target (`owl.entity_links`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateEntityLinks(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Run the timer triggers and the overlap triggers against the primary player (`owl.triggers`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateTriggers(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Place the sound listener and the spatial sound sources (`owl.sound`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateSound(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Advance the `AnimatedSpriteRenderer` frames (`owl.sprite_animation`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateAnimatedSprites(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Draw the end-of-game message ("Victory!" / "You loose!") once the game is won or lost (`owl.game_over`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void renderGameOver(Scene& ioScene, const SystemContext& iContext);

}// namespace owl::scene::systems
