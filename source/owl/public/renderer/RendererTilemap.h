/**
 * @file RendererTilemap.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/Transform.h"
#include "renderer/Camera.h"
#include "renderer/gpu/Texture.h"

#include <cstddef>
#include <cstdint>

namespace owl::scene {
class TilemapAsset;
}// namespace owl::scene

namespace owl::renderer {

/**
 * @brief
 *  Persistent, chunked and culled 2D tilemap renderer.
 *
 *  Every layer of a tilemap asset is cut into `TilemapLayer::chunkSize`² chunks. Each non-empty chunk owns a
 *  persistent per-instance GPU buffer (one `{cell, tileIndex}` entry per non-empty cell), cached per asset and
 *  rebuilt only when the chunk generation of the layer changes: a static map uploads nothing per frame, a one-cell
 *  edit uploads one chunk. The vertex shader rebuilds the cell corners from the per-draw uniform block (camera,
 *  entity world transform, grid origin, cell size, layer depth, parallax offset, atlas grid) and samples the
 *  tileset atlas bound to the draw's texture slot.
 *
 * Lifecycle:
 *   - `init()` builds the pipeline / shader / uniform buffer once at engine startup.
 *   - `beginPrepare()` then `prepareTilemap(asset)` for each tilemap, before any render layer draws (the uploads
 *     stay outside the render passes); `Scene::renderWithStack` runs them.
 *   - `beginScene(camera)` (called by `Renderer2D::beginScene`) latches the camera and clears the queue.
 *   - `drawTilemap(asset, transform, entityId)` queues one entity (from the render layer's `onRender`).
 *   - `flushPending()` (called by `Renderer2D::flush` inside its batch) culls the chunks against the view and
 *     draws the visible ones, layer by layer, in painter's order.
 *   - `shutdown()` releases the pipeline, the cache and the buffers.
 *
 * Backends: Vulkan and OpenGL share `tilemap_instanced.slang`; the Null backend records nothing but the CPU
 * work (chunk builds, culling, statistics) runs the same.
 */
class OWL_API RendererTilemap final {
public:
	RendererTilemap() = delete;

	RendererTilemap(const RendererTilemap&) = delete;

	RendererTilemap(RendererTilemap&&) = delete;

	auto operator=(const RendererTilemap&) -> RendererTilemap& = delete;

	auto operator=(RendererTilemap&&) -> RendererTilemap& = delete;

	~RendererTilemap() = delete;

	/**
	 * @brief
	 *  One-shot startup: compile the Slang shader, allocate the per-draw uniform buffer.
	 *  Safe to call multiple times — idempotent.
	 */
	static void init();

	/**
	 * @brief
	 *  Tear-down: release the pipeline, the chunk cache and the buffers. Safe to call multiple times.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Open a prepare phase: reset the prepare statistics and evict the cached assets no prepare nor draw has used
	 *  for `cacheEvictionPrepares` phases.
	 */
	static void beginPrepare();

	/**
	 * @brief
	 *  Build and upload every chunk of the asset whose generation changed since its last upload.
	 *
	 *  Call it before any render layer draws, so the uploads never split a render pass. A chunk left stale (an asset
	 *  drawn without a prepare) is built by `flushPending` instead, and counted in `lateBuiltChunkCount`.
	 * @param[in] iAsset The tilemap asset.
	 */
	static void prepareTilemap(const scene::TilemapAsset& iAsset);

	/**
	 * @brief
	 *  Latch the camera view-projection and position for this scene and reset the per-frame statistics.
	 *  Mirrors `Renderer2D::beginScene`.
	 * @param[in] iCamera The active camera.
	 */
	static void beginScene(const Camera& iCamera);

	/**
	 * @brief
	 *  Flush any deferred state. A no-op kept for symmetry with `Renderer2D`; the queued tilemaps are emitted by
	 *  `flushPending`, which `Renderer2D::flush` calls from inside its render-pass batch.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Queue one tilemap entity for rendering this frame.
	 *
	 * The draw is **deferred**: the tilemap uses its own pipeline but must composite in painter's order inside the
	 * same `beginBatch`/`endBatch` render pass as the `Renderer2D` background and sprites. `Renderer2D::flush`
	 * drains the queue via `flushPending` after the background and before the sprite batch.
	 * @param[in] iAsset The tilemap asset (must have a resolved tileset). The reference must stay valid until
	 *  `flushPending` runs this frame.
	 * @param[in] iWorldTransform Entity world transform (translation, rotation and scale are all applied).
	 * @param[in] iEntityId Entity id written into the picking render target.
	 */
	static void drawTilemap(const scene::TilemapAsset& iAsset, const math::Transform& iWorldTransform, int iEntityId);

	/**
	 * @brief
	 *  Emit every tilemap queued by `drawTilemap` this frame: for each entity and each visible layer, one uniform
	 *  update and one instanced draw per non-empty chunk inside the view. Distinct tilesets are bound to distinct
	 *  slots of the shader's texture array (up to 32). Must be called from inside an active
	 *  `beginBatch`/`endBatch` render pass. Clears the queue.
	 */
	static void flushPending();

	/**
	 * @brief
	 *  Drop every cached chunk buffer (the next prepare rebuilds what is still drawn).
	 */
	static void clearCache();

	/**
	 * @brief
	 *  Number of tilemap assets holding chunk buffers in the cache.
	 * @return The cached asset count.
	 */
	[[nodiscard]] static auto getCachedAssetCount() -> size_t;

	/// Prepare phases an unused asset stays cached before `beginPrepare` evicts it.
	static constexpr uint64_t cacheEvictionPrepares = 600;

	/**
	 * @brief
	 *  Per-frame draw counters reset by `beginScene` and updated by `flushPending`.
	 */
	struct Statistics {
		/// Instanced draw calls issued (one per visible non-empty chunk of a visible layer).
		uint32_t drawCallCount = 0;
		/// Non-empty cells drawn (sum of the instance counts of the drawn chunks).
		uint32_t instanceCount = 0;
		/// Layers walked, including the hidden and empty ones.
		uint32_t layerCount = 0;
		/// Non-empty chunks skipped because they lie outside the view.
		uint32_t culledChunkCount = 0;
	};

	/**
	 * @brief
	 *  Upload counters reset by `beginPrepare` and updated by `prepareTilemap` (and by late builds in
	 *  `flushPending`).
	 */
	struct PrepareStatistics {
		/// Chunks built and uploaded.
		uint32_t builtChunkCount = 0;
		/// Chunks built by `flushPending` because no prepare ran for them (uploads inside the render pass).
		uint32_t lateBuiltChunkCount = 0;
		/// Bytes sent to the GPU buffers of the chunks.
		uint64_t uploadedBytes = 0;
	};

	/**
	 * @brief
	 *  Read the current frame's draw statistics.
	 * @return Per-frame counters.
	 */
	[[nodiscard]] static auto getStatistics() -> Statistics;

	/**
	 * @brief
	 *  Read the upload statistics since the last `beginPrepare`.
	 * @return Upload counters.
	 */
	[[nodiscard]] static auto getPrepareStatistics() -> PrepareStatistics;

	/**
	 * @brief
	 *  Reset the per-frame draw statistics. Called by `beginScene`.
	 */
	static void resetStats();
};

}// namespace owl::renderer
