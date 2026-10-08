/**
 * @file RendererVoxel.h
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/Transform.h"
#include "math/vectors.h"
#include "renderer/Camera.h"

#include <cstdint>
#include <optional>

namespace owl::scene::component {
struct VoxelWorld;
}// namespace owl::scene::component

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
 *  How chunk meshes are built and uploaded.
 */
struct VoxelMeshingConfig {
	/// Mesh on the task scheduler workers; false (or no `Application`) meshes and uploads every chunk in the frame.
	bool async = true;
	/// Upper bound of chunk meshes uploaded per `beginPrepare` window (at least one upload always proceeds).
	uint32_t maxUploadsPerFrame = 32;
	/// Upload time budget per `beginPrepare` window, in milliseconds (checked between two uploads).
	float uploadBudgetMs = 2.f;
	/// Upper bound of meshing jobs queued or running at once (bounds the captured copies and keeps priorities fresh).
	uint32_t maxJobsInFlight = 32;
};

class RendererVoxel;

/**
 * @brief
 *  GPU meshes of the voxel worlds of one engine context, keyed by entity and chunk, and their meshing jobs.
 *
 * Owned by `app::EngineContext`, so two applications (or two isolated tests) never share meshes. Release it before
 * the renderer shuts down: it holds GPU buffers.
 */
class OWL_API VoxelMeshCache final {
public:
	/**
	 * @brief
	 *  Create an empty cache.
	 */
	VoxelMeshCache();

	/**
	 * @brief
	 *  Destroy the cache and its GPU meshes.
	 */
	~VoxelMeshCache();

	VoxelMeshCache(const VoxelMeshCache&) = delete;

	VoxelMeshCache(VoxelMeshCache&&) = delete;

	auto operator=(const VoxelMeshCache&) -> VoxelMeshCache& = delete;

	auto operator=(VoxelMeshCache&&) -> VoxelMeshCache& = delete;

	/**
	 * @brief
	 *  Drop all cached meshes and the finished meshes not uploaded yet (call on scene transitions to avoid stale
	 *  geometry). A job still running delivers later and is installed only if its chunk revision still matches.
	 */
	void clear();

	/// Cache contents (opaque, defined by the renderer).
	struct Data;

private:
	friend class RendererVoxel;
	/// The contents.
	uniq<Data> mp_data;
};

/**
 * @brief
 *  Draws `scene::component::VoxelWorld` entities in 3D on top of `Renderer3D`.
 *
 * Each chunk is greedy-meshed (`ChunkMesher`) and uploaded via `Renderer3D::createMesh` using the frac-tiled `voxel`
 * shader; the GPU mesh is cached per entity+chunk and rebuilt when the chunk revision changes. By default meshing
 * runs on the task scheduler workers: the main thread captures an immutable `data::voxel::ChunkNeighborhood`
 * (nearest chunks first), a worker meshes it into upload-ready vertices, and the main thread uploads the result
 * under a per-frame budget, outside any render pass (`prepareWorld`). A result whose chunk changed or unloaded
 * meanwhile is dropped (revision check); the previous mesh stays drawn until its replacement is uploaded. Block
 * textures are resolved (Nearest filtering) and bound per draw. A static facade mirroring the other renderers; the
 * actual GPU work is delegated to `Renderer3D`.
 */
class OWL_API RendererVoxel final {
public:
	/**
	 * @brief
	 *  Mesh counters, for diagnostics, benchmarks and headless tests.
	 */
	struct Statistics {
		/// GPU chunk meshes (opaque + transparent) currently cached across every voxel world.
		uint32_t cachedMeshCount = 0;
		/// Chunk meshes submitted to `Renderer3D` since the last `beginScene`.
		uint32_t drawnMeshCount = 0;
		/// Meshing jobs queued or running on the task scheduler.
		uint32_t pendingJobCount = 0;
		/// Finished meshes waiting for an upload slot.
		uint32_t readyMeshCount = 0;
		/// Chunk meshes uploaded since the last `beginPrepare`.
		uint32_t uploadedThisFrame = 0;
		/// Chunk meshes uploaded since the cache was created.
		uint64_t uploadedMeshCount = 0;
		/// Meshing results dropped since the cache was created because their chunk changed or unloaded meanwhile.
		uint64_t discardedMeshCount = 0;
		/// Time spent meshing since the cache was created (workers and synchronous path), in nanoseconds.
		uint64_t meshingNs = 0;
		/// Sum of the latencies from a chunk needing a mesh to its upload, in nanoseconds.
		uint64_t latencyNsTotal = 0;
		/// Longest such latency, in nanoseconds.
		uint64_t latencyNsMax = 0;
		/// Number of latencies summed in `latencyNsTotal`.
		uint64_t latencyCount = 0;
	};

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
	 *  Open a new upload budget window (call once per frame, before the `prepareWorld` calls of that frame).
	 * @param[in,out] ioCache The mesh cache.
	 */
	static void beginPrepare(VoxelMeshCache& ioCache);

	/**
	 * @brief
	 *  Bring the cached GPU meshes of a voxel world up to date.
	 *
	 * Uploads finished meshes within the budget of the current `beginPrepare` window, drops the meshes of
	 * unloaded chunks, then queues a meshing job for every chunk whose revision has no mesh yet, nearest to the
	 * camera first. A chunk is held back while one of its neighbours is still being generated, since that
	 * neighbour's arrival would invalidate the mesh. In synchronous mode every chunk is meshed and uploaded here.
	 *
	 * Must be called **outside** any render pass (`Scene::renderWithStack` does it before the first layer): it
	 * creates GPU buffers, pipelines and textures, which submit single-time command buffers and therefore must not
	 * run while a frame's command buffer is being recorded. `drawVoxelWorld` then only binds and draws these cached
	 * resources.
	 * @param[in,out] ioCache The mesh cache.
	 * @param[in,out] ioComponent The voxel world component (its chunks are only read).
	 * @param[in] iEntityId The entity id (keys the per-entity mesh cache).
	 */
	static void prepareWorld(VoxelMeshCache& ioCache, scene::component::VoxelWorld& ioComponent, int iEntityId);

	/**
	 * @brief
	 *  Draw one voxel world entity from its cached meshes (built by `prepareWorld`).
	 * @param[in,out] ioCache The mesh cache.
	 * @param[in,out] ioComponent The voxel world component.
	 * @param[in] iWorldTransform The entity world transform.
	 * @param[in] iEntityId The entity id (keys the per-entity mesh cache).
	 */
	static void drawVoxelWorld(VoxelMeshCache& ioCache, scene::component::VoxelWorld& ioComponent,
							   const math::Transform& iWorldTransform, int iEntityId);

	/**
	 * @brief
	 *  Read the mesh counters.
	 * @param[in] iCache The mesh cache.
	 * @return The cached mesh count, the meshes drawn since the last `beginScene` and the meshing counters.
	 */
	[[nodiscard]] static auto getStatistics(const VoxelMeshCache& iCache) -> Statistics;

	/**
	 * @brief
	 *  Chunk revision the cached mesh of a chunk was built from (diagnostics and tests).
	 * @param[in] iCache The mesh cache.
	 * @param[in] iEntityId The voxel world entity id.
	 * @param[in] iCoord The chunk coordinate.
	 * @return The revision of the uploaded mesh, or `std::nullopt` while the chunk has no mesh yet.
	 */
	[[nodiscard]] static auto getMeshedRevision(const VoxelMeshCache& iCache, int iEntityId, const math::vec3i& iCoord)
			-> std::optional<uint64_t>;

	/**
	 * @brief
	 *  Set how chunk meshes are built and uploaded (shared by every cache).
	 * @param[in] iConfig The meshing configuration.
	 */
	static void setMeshingConfig(const VoxelMeshingConfig& iConfig);

	/**
	 * @brief
	 *  The current meshing configuration.
	 * @return The meshing configuration.
	 */
	[[nodiscard]] static auto getMeshingConfig() -> const VoxelMeshingConfig&;
};

}// namespace owl::renderer
