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
 *  GPU-instanced 2D tilemap renderer. Replaces the per-cell `drawQuad` loop
 *  used by `Scene::drawTilemapQuads`: one `vkCmdDrawIndexed(6, cellCount, …)` /
 *  `glDrawElementsInstanced(GL_TRIANGLES, 6, cellCount, …)` per visible layer.
 *
 *  The per-vertex VBO carries a single 4-vertex quad (corner indices 0..3).
 *  The per-instance VBO is rebuilt every frame, one entry per non-empty
 *  cell with `{cellWorldPos.xy, tileIndex, entityId}`. The vertex shader
 *  synthesises the world corner position and the atlas UV from a small
 *  per-draw UBO (atlas columns/rows, cell size, layer Z, tint, texture slot,
 *  half-texel inset) — no CPU-side per-cell UV computation.
 *
 * Lifecycle mirrors `Renderer2D`:
 *   - `init()` builds the pipeline / shader / static per-vertex buffer once
 *     at engine startup.
 *   - `beginScene(camera)` uploads the camera UBO.
 *   - `drawTilemap(asset, transform, entityId)` iterates layers and emits one
 *     instanced draw per visible non-empty layer.
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
	 *  per-vertex VBO (4 corners), allocate the camera + per-draw UBOs.
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
	 *  Flush any deferred state. Currently a no-op (every `drawTilemap`
	 *  issues its draws inline) but kept for symmetry with `Renderer2D`
	 *  and to give the implementation room to batch later if needed.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Submit one tilemap entity for rendering. Iterates every visible
	 *  layer and emits one instanced drawcall per layer.
	 * @param[in] iAsset The tilemap asset (must have a resolved tileset).
	 * @param[in] iWorldTransform Entity world transform — translation of the
	 *  tilemap origin, no per-cell scaling.
	 * @param[in] iEntityId Entity id written into the picking render target.
	 */
	static void drawTilemap(const scene::TilemapAsset& iAsset, const math::Transform& iWorldTransform, int iEntityId);

	/**
	 * @brief
	 *  Per-frame counters reset by `beginScene` and updated by every
	 *  `drawTilemap` call. Exposed for profiling overlays.
	 */
	struct Statistics {
		/// Number of instanced drawcalls (one per visible non-empty layer).
		uint32_t drawCallCount = 0;
		/// Sum of `cellCount` across every drawcall this frame.
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
