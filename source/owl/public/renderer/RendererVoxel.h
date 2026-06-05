/**
 * @file RendererVoxel.h
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/Transform.h"
#include "math/vectors.h"
#include "renderer/Camera.h"
#include "scene/component/VoxelWorld.h"

namespace owl::renderer {

/**
 * @brief
 *  Per-layer configuration for the voxel renderer.
 */
struct VoxelConfig {
	/// World-space direction the sun light travels.
	math::vec3 sunDirection{-0.4f, -1.f, -0.6f};
	/// Ambient light colour added before the directional term.
	math::vec3 ambient{0.35f, 0.35f, 0.4f};
};

/**
 * @brief
 *  Draws `scene::component::VoxelWorld` entities in 3D on top of `Renderer3D`.
 *
 * Each chunk is greedy-meshed (`ChunkMesher`) and uploaded once via
 * `Renderer3D::createMesh` using the frac-tiled `voxel` shader; the GPU mesh is
 * cached per entity+chunk and rebuilt only when the chunk is dirty. Block
 * textures are resolved (Nearest filtering) and bound per draw. A static facade
 * mirroring the other renderers; the actual GPU work is delegated to
 * `Renderer3D`.
 */
class OWL_API RendererVoxel final {
public:
	/**
	 * @brief
	 *  Initialize the renderer (resets the mesh / texture caches).
	 */
	static void init();

	/**
	 * @brief
	 *  Release cached meshes and textures.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Begin a frame: bind the camera and push the lighting to `Renderer3D`.
	 * @param[in] iCamera The camera whose view-projection drives the frame.
	 * @param[in] iConfig The voxel layer configuration (lighting).
	 */
	static void beginScene(const Camera& iCamera, const VoxelConfig& iConfig);

	/**
	 * @brief
	 *  End a frame (restores depth state via `Renderer3D`).
	 */
	static void endScene();

	/**
	 * @brief
	 *  Build and cache the GPU meshes and textures for a voxel world.
	 *
	 *  Must be called **outside** any render pass (e.g. from `Scene::onStartRuntime`): it creates GPU buffers,
	 *  pipelines and textures, which submit single-time command buffers and therefore must not run while a frame's
	 *  command buffer is being recorded. `drawVoxelWorld` then only binds and draws these cached resources.
	 * @param[in,out] ioComponent The voxel world component (chunks are marked clean as they are meshed).
	 * @param[in] iEntityId The entity id (keys the per-entity mesh cache).
	 */
	static void prepareWorld(scene::component::VoxelWorld& ioComponent, int iEntityId);

	/**
	 * @brief
	 *  Draw one voxel world entity from its cached meshes (built by `prepareWorld`).
	 * @param[in,out] ioComponent The voxel world component.
	 * @param[in] iWorldTransform The entity world transform.
	 * @param[in] iEntityId The entity id (keys the per-entity mesh cache).
	 */
	static void drawVoxelWorld(scene::component::VoxelWorld& ioComponent, const math::Transform& iWorldTransform,
							   int iEntityId);

	/**
	 * @brief
	 *  Drop all cached meshes (call on scene transitions to avoid stale geometry).
	 */
	static void clearCache();
};

}// namespace owl::renderer
