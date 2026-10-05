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
 *  Lua scripting benchmarks.
 * @param[in,out] ioRunner The runner.
 */
void runScriptBenches(Runner& ioRunner);

/**
 * @brief
 *  Box2D physics benchmarks.
 * @param[in,out] ioRunner The runner.
 */
void runPhysicsBenches(Runner& ioRunner);

/**
 * @brief
 *  Load and editor-frame benchmarks on the scenes of `sample_project/` (Null backend).
 * @param[in,out] ioRunner The runner.
 */
void runSampleBenches(Runner& ioRunner);

/**
 * @brief
 *  Slang shader compilation and SPIR-V cache benchmarks.
 * @param[in,out] ioRunner The runner.
 */
void runSlangBenches(Runner& ioRunner);

}// namespace owl::bench
