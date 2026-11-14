/**
 * @file Cases.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "harness/Bench.h"

#include <core/Core.h>
#include <scene/Scene.h>

#include <cstdint>

/**
 * @brief
 *  Benchmark groups. Each `run*` function registers and runs the cases of one engine subsystem.
 */
namespace owl::bench {

/**
 * @brief
 *  Shape of a synthetic scene hierarchy.
 */
enum struct Shape : uint8_t {
	Flat,///< Every entity is a root.
	Chain,///< Each entity is the child of the previous one.
	Wide,///< One root, every other entity is its direct child.
	Forest///< Roots of 100 direct children each.
};

/**
 * @brief
 *  Build a synthetic scene of sprite entities, each translated by +1 on X in its parent space.
 * @param[in] iCount Number of entities.
 * @param[in] iShape Hierarchy shape.
 * @return The scene.
 */
auto makeSpriteScene(uint32_t iCount, Shape iShape) -> shared<scene::Scene>;

/**
 * @brief
 *  Scene benchmarks: entity creation, world transforms, views, copy.
 * @param[in,out] ioRunner The runner.
 */
void runSceneBenches(Runner& ioRunner);

/**
 * @brief
 *  Serialization benchmarks: YAML scene and entity round trips, prefab instantiation.
 * @param[in,out] ioRunner The runner.
 */
void runSerializeBenches(Runner& ioRunner);

/**
 * @brief
 *  Renderer2D batching benchmarks on the Null backend.
 * @param[in,out] ioRunner The runner.
 */
void runRenderer2DBenches(Runner& ioRunner);

/**
 * @brief
 *  HUD benchmarks on the Null backend: layout and HUD pass of 500 widgets.
 * @param[in,out] ioRunner The runner.
 */
void runHudBenches(Runner& ioRunner);

/**
 * @brief
 *  Chunked tilemap benchmarks on the Null backend: static frames, one-cell edit, chunk build.
 * @param[in,out] ioRunner The runner.
 */
void runTilemapBenches(Runner& ioRunner);

/**
 * @brief
 *  Isometric terrain: slope-aware picking against its 20 microsecond budget.
 * @param[in,out] ioRunner The bench runner.
 */
void runIsometricBenches(Runner& ioRunner);

/**
 * @brief
 *  Tile-graph pathfinding on a 1024x1024 map: graph build, long queries (flat and hierarchical), one-cell edits.
 * @param[in,out] ioRunner The bench runner.
 */
void runPathBenches(Runner& ioRunner);

/**
 * @brief
 *  Whole-frame CPU benchmarks (editor update, runtime tick) on the Null backend.
 * @param[in,out] ioRunner The runner.
 */
void runFrameBenches(Runner& ioRunner);

/**
 * @brief
 *  Voxel benchmarks: terrain generation and greedy meshing.
 * @param[in,out] ioRunner The runner.
 */
void runVoxelBenches(Runner& ioRunner);

/**
 * @brief
 *  Voxel streaming frames: main-thread cost per frame while terrain streams in (Play walk, editor load).
 * @param[in,out] ioRunner The runner.
 */
void runVoxelStreamingBenches(Runner& ioRunner);

/**
 * @brief
 *  Lua scripting benchmarks.
 * @param[in,out] ioRunner The runner.
 */
void runScriptBenches(Runner& ioRunner);

/**
 * @brief
 *  Physics benchmarks (Jolt): world creation and steps of 2D boxes (one to eight workers, two collision steps, and one
 *  collision step at 1000 bodies), rollback snapshots, gravity fields and trajectory prediction, time warp, hinged
 *  chains and compound assemblies. Budgets: 1 ms for the step of 1000 falling 2D boxes on one thread with one collision
 *  step, 1.5 ms at 5000 with four workers; 500 us for a snapshot or a restore of 1000 bodies; 100 us for the step of
 *  100 bodies in four fields, 50 us for a prediction of 1000 steps; 1 ms for a frame at x80 or a hand-off of 100
 *  bodies; 1 ms for the step of 10 chains of 20 hinged links, 250 us for 50 assemblies of 8 parts, 50 us for a part
 *  detached and attached again.
 * @param[in,out] ioRunner The runner.
 */
void runPhysicsBenches(Runner& ioRunner);

/**
 * @brief
 *  Load and editor-frame benchmarks on the scenes of `engine_demo/` (Null backend).
 * @param[in,out] ioRunner The runner.
 */
void runSampleBenches(Runner& ioRunner);

/**
 * @brief
 *  Slang shader compilation and SPIR-V cache benchmarks.
 * @param[in,out] ioRunner The runner.
 */
void runSlangBenches(Runner& ioRunner);

/**
 * @brief
 *  MCP round trips over loopback HTTP into a headless editor session, the main loop pumping the server.
 * @param[in,out] ioRunner The runner.
 */
void runMcpBenches(Runner& ioRunner);

/**
 * @brief
 *  Spatial culling: loose octree insertion, frustum queries (against a brute-force pass) and moves, and the grouped,
 *  culled flush of the opaque meshes of `RendererMesh` (CPU side, Null backend).
 * @param[in,out] ioRunner The runner.
 */
void runCullingBenches(Runner& ioRunner);

/**
 * @brief
 *  Procedural sky: drawing the analytic sky and prefiltering it into an environment, the work a moving sun redoes on a
 *  worker.
 * @param[in,out] ioRunner The runner.
 */
void runSkyBenches(Runner& ioRunner);

/**
 * @brief
 *  Particles: one frame of the CPU simulation (the reference of the compute shader, and the Null-backend path) for
 *  1000 to 65536 particles, and the per-frame bookkeeping of 256 GPU emitters; budget 2 ms for 65536 CPU particles,
 *  50 us for the 256 emitters.
 * @param[in,out] ioRunner The runner.
 */
void runParticleBenches(Runner& ioRunner);

/**
 * @brief
 *  Large worlds: the quadtree selection of an Earth-sized planet near its ground, the meshing of one chunk, the frame
 *  of a planet whose chunks are built, and the frame of a world partition of 4096 cells; budget 200 us for the planet
 *  frame, 100 us for the partition frame.
 * @param[in,out] ioRunner The runner.
 */
void runLargeWorldBenches(Runner& ioRunner);

/**
 * @brief
 *  Splines: rebuilding the arc-length table of a 64-point curve, sampling it by distance (a rail of followers) and
 *  finding the nearest distance to a point; budget 1 us per sample.
 * @param[in,out] ioRunner The runner.
 */
void runSplineBenches(Runner& ioRunner);

/**
 * @brief
 *  Navigation meshes: baking a room of 24 x 24 with 36 crates at a cell of 0.1, a path across it, and the avoidance
 *  velocities of 64 bodies for one tick; budget 50 us for the path, 1 ms for the 64 bodies.
 * @param[in,out] ioRunner The runner.
 */
void runNavBenches(Runner& ioRunner);

/**
 * @brief
 *  AI: one tick of 1000 behaviour trees (a reactive guard: chase in reach, else wander, wait and count), and 10 runtime
 *  frames of a 2D flock of 1000 steering agents (separation, alignment, cohesion, wander, avoidance). Budgets: 200 us
 *  for the 1000 trees, 2 ms per frame for the 1000 agents.
 * @param[in,out] ioRunner The runner.
 */
void runAiBenches(Runner& ioRunner);

/**
 * @brief
 *  Replays: 60 runtime frames of 100 falling boxes live, recorded and played back, the rewind of the editor (scene copy,
 *  start on the timeline, 60 frames simulated again) and the world hash of a checkpoint over 1000 bodies. Budgets: a
 *  recorded or replayed frame within 10 % of a live one, 50 ms to simulate one second of play again, 5 ms for the hash
 *  of 1000 bodies.
 * @param[in,out] ioRunner The runner.
 */
void runReplayBenches(Runner& ioRunner);

}// namespace owl::bench
