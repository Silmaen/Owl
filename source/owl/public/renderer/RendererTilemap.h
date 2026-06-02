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

namespace owl::scene {
class TilemapAsset;
}// namespace owl::scene

namespace owl::renderer {

/**
 * @brief
 *  GPU-instanced 2D tilemap renderer. Replaces the per-cell `drawQuad` loop the
 *  scene used to run: every queued tilemap (all entities, all visible layers)
 *  is drawn in a single `vkCmdDrawIndexed(6, cellCount, …)` /
 *  `glDrawElementsInstanced(GL_TRIANGLES, 6, cellCount, …)`.
 *
 *  The per-vertex VBO carries a single 4-vertex quad (corner indices 0..3).
 *  The per-instance VBO is rebuilt every frame, one entry per non-empty cell
 *  carrying everything that cell needs — `{cellWorldPos.xy, tileIndex,
 *  entityId, layerZ, cellSize, atlasColumns, atlasRows, halfTexel,
 *  textureSlot}`. The vertex shader synthesises the world corner position and
 *  the atlas UV from those attributes (no CPU-side per-cell UV computation, no
 *  per-draw UBO); distinct tilesets bind to distinct slots of the shader's
 *  32-texture array.
 *
 * Lifecycle mirrors `Renderer2D`:
 *   - `init()` builds the pipeline / shader / static per-vertex buffer once
 *     at engine startup.
 *   - `beginScene(camera)` uploads the camera UBO and clears the queue.
 *   - `drawTilemap(asset, transform, entityId)` queues one entity.
 *   - `flushPending()` emits every queued tilemap in one instanced draw — called
 *     by `Renderer2D::flush` from inside its render-pass batch.
 *   - `endScene()` is a no-op kept for symmetry.
 *   - `shutdown()` releases the pipeline / buffers.
 *
 * Backends: Vulkan and OpenGL share the same `tilemap_instanced.slang`
 * source compiled to SPIR-V at runtime; the Null backend is a no-op.
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
	 *  One-shot startup: compile the Slang shader, create the static
	 *  per-vertex VBO (4 corners), allocate the camera UBO.
	 *  Safe to call multiple times — idempotent.
	 */
	static void init();

	/**
	 * @brief
	 *  Tear-down: release the pipeline, buffers, and shader. Safe to call
	 *  multiple times.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Upload the camera view-projection to the per-frame UBO and reset
	 *  the per-frame stats. Mirrors `Renderer2D::beginScene`.
	 * @param[in] iCamera The active camera.
	 */
	static void beginScene(const Camera& iCamera);

	/**
	 * @brief
	 *  Flush any deferred state. A no-op kept for symmetry with
	 *  `Renderer2D`; the queued tilemaps are emitted by `flushPending`,
	 *  which `Renderer2D::flush` calls from inside its render-pass batch.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Queue one tilemap entity for rendering this frame.
	 *
	 * The draw is **deferred**, not issued immediately: the tilemap uses its
	 * own pipeline but must composite in painter's order inside the same
	 * `beginBatch`/`endBatch` render pass as the `Renderer2D` background and
	 * sprites. `Renderer2D::flush` drains the queue via `flushPending` after
	 * the background and before the sprite batch. Issuing the draw here (mid
	 * `Scene::render`, before the batch opens) would record into a command
	 * buffer with no active render pass — on Vulkan that hangs the GPU.
	 * @param[in] iAsset The tilemap asset (must have a resolved tileset). The
	 *  reference must stay valid until `flushPending` runs this frame.
	 * @param[in] iWorldTransform Entity world transform — translation of the
	 *  tilemap origin, no per-cell scaling.
	 * @param[in] iEntityId Entity id written into the picking render target.
	 */
	static void drawTilemap(const scene::TilemapAsset& iAsset, const math::Transform& iWorldTransform, int iEntityId);

	/**
	 * @brief
	 *  Emit every tilemap queued by `drawTilemap` this frame in a single
	 *  instanced drawcall. All entities and all their visible layers share one
	 *  instance buffer — each cell carries its own painter's-order `layerZ`,
	 *  atlas metadata and texture slot, and distinct tilesets are bound to
	 *  distinct slots in the shader's texture array (up to 32). Must be called
	 *  from inside an active `beginBatch`/`endBatch` render pass —
	 *  `Renderer2D::flush` invokes it between the background and the sprite
	 *  draws so the tilemap composites in the correct painter's order. Clears
	 *  the queue.
	 */
	static void flushPending();

	/**
	 * @brief
	 *  Per-frame counters reset by `beginScene` and updated by `flushPending`.
	 *  Exposed for profiling overlays.
	 */
	struct Statistics {
		/// Number of instanced drawcalls (0 if no cells, else 1 — all tilemaps combine into one draw).
		uint32_t drawCallCount = 0;
		/// Total non-empty cells drawn across every queued tilemap this frame.
		uint32_t instanceCount = 0;
		/**
		 * @brief
		 *  Number of layers walked — including empty ones that
		 *  short-circuit before drawing.
		 */
		uint32_t layerCount = 0;
	};

	/**
	 * @brief
	 *  Read the current frame's statistics.
	 * @return Per-frame counters.
	 */
	[[nodiscard]] static auto getStatistics() -> Statistics;

	/**
	 * @brief
	 *  Reset the per-frame statistics counter. Called by `beginScene`.
	 */
	static void resetStats();
};

}// namespace owl::renderer
