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
 *  Run `NativeScript` and `LuaScript` fixed ticks of the visible entities (`owl.scripts`, `on_tick`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The tick context.
 */
void tickScripts(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Move the entities with an `Oscillator` that are not simulated bodies (`owl.oscillators`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateOscillators(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Drive the simulated bodies with an `Oscillator` through their velocity, once per tick (`owl.oscillators`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The tick context.
 */
void tickOscillators(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Walk the entities with a `Patrol` along their waypoints or around their post (`owl.patrols`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updatePatrols(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Move the entities with a `Drift` (`owl.drift`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateDrifts(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Place the entities with a `FollowTarget` at their offset from the target (`owl.follow_target`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateFollowTargets(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Advance the `DayNightCycle`s (`owl.day_night`), then light the scene for their time of day.
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateDayNightCycles(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Turn the sun light and the sky of every `DayNightCycle` for its time of day, without advancing it (Owl Nest).
 * @param[in,out] ioScene The scene.
 * @param[in] iRuntime Whether the scene plays: the night exposure of the cameras applies only then.
 */
void applyDayNightCycles(Scene& ioScene, bool iRuntime = false);

/**
 * @brief
 *  Walk the entities with a `FirstPersonController` (`owl.first_person`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateFirstPersonControllers(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Make the lights with a `LightFlicker` flicker from the scene time (`owl.light_flicker`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateLightFlickers(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Advance the particle emitters and record the trails of the scene in Play (`owl.particles`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateParticles(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Advance the particle emitters and the trails by a frame.
 *
 * A system is made for each emitter the first time; in Play it starts when its emitter plays on start, in Edit mode it
 * always starts (the viewport preview), and its pause and restart come from the inspector.
 * @param[in,out] ioScene The scene.
 * @param[in] iSeconds Duration of the frame.
 * @param[in] iEditorMode Whether the scene is edited (the preview) or played.
 */
OWL_API void advanceEffects(Scene& ioScene, float iSeconds, bool iEditorMode);

/**
 * @brief
 *  Spend the frame's budget on the pending path requests of the scene (`owl.navigation`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateNavigation(Scene& ioScene, const SystemContext& iContext);

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
 * the kinematic Box2D body. Lua scripts that bypass the built-in path (by emptying `interactionAction`) drive
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
 *  Run one step of the scene's physics world, write the transforms, then dispatch the begun collisions to the
 *  scripts (`owl.physics` tick).
 *
 * The last tick of the frame writes the drawn (interpolated) poses, the others the simulated ones so the next tick
 * reads where the bodies are.
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The tick context.
 */
void tickPhysics(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Blend the drawn poses of a frame that ran no tick (`owl.physics` update).
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
 *  Synchronise the world positions, then move the world origin onto the primary camera when it is farther than the
 *  shift radius (`owl.floating_origin`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateFloatingOrigin(Scene& ioScene, const SystemContext& iContext);

/**
 * @brief
 *  Stream the world partition cells around the streaming sources, or the primary camera without one
 *  (`owl.world_streaming`).
 * @param[in,out] ioScene The scene.
 * @param[in] iContext The frame context.
 */
void updateWorldStreaming(Scene& ioScene, const SystemContext& iContext);

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
