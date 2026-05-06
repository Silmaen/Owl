/**
 * @file RendererRaycast.h
 * @author Silmaen
 * @date 04/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include "renderer/Camera.h"

namespace owl::scene::component {

struct Tilemap;
}// namespace owl::scene::component

namespace owl::math {

class Transform;
}// namespace owl::math

/**
 * @brief
 *  Wolfenstein-style raycaster family.
 *
 * Hosts `RendererRaycast` (static facade running per-column DDA across a
 * `scene::component::Tilemap` to produce a pseudo-3D first-person view)
 * and its `RenderLayer` adapter `RendererRaycastLayer`. Configuration is
 * driven by `RaycastConfig` (FOV, max distance, sky / floor colours, ray
 * count). The implementation in v0.2.0 emits textured wall stripes via the
 * `owl::renderer::Renderer2D` batch — a future PR can move the
 * inner loop into a dedicated full-screen Slang shader.
 */
namespace owl::renderer {
/**
 * @brief
 *  Configuration for one raycaster pass.
 *
 * Holds the parameters that change between scene-activations: field of view,
 * clipping range, sky / floor colours. The raycaster operates on a 2D top-down
 * grid (a `scene::component::Tilemap`) and synthesises a pseudo-3D first-person
 * view by per-column DDA.
 */
struct OWL_API RaycastConfig {
	/// Horizontal field-of-view, in degrees.
	float fovDegrees = 75.f;
	/// Maximum DDA step distance, in cells. Walls past this distance are not drawn.
	float maxDistance = 64.f;
	/// Solid colour drawn above the wall horizon (sky / ceiling).
	math::vec4 ceilingColor{0.18f, 0.20f, 0.30f, 1.f};
	/// Solid colour drawn below the wall horizon (floor).
	math::vec4 floorColor{0.20f, 0.16f, 0.12f, 1.f};
	/// Number of casted rays (= rendered vertical stripes). 0 = derive from viewport width.
	uint32_t numRays = 0;
};

/**
 * @brief
 *  Static facade for a CPU-DDA, GPU-textured raycasting renderer.
 *
 * Mirrors the `Renderer2D` shape: a single static class with `init`/`shutdown`
 * book-keeping plus `beginScene` / `draw...` / `endScene` per frame. Internally
 * this v0.2.0 implementation rasterises wall stripes by emitting textured
 * `Renderer2D::drawQuad` calls — the heavy lifting (DDA traversal, perspective
 * stripe height, atlas UV slicing) happens on the CPU, drawing happens through
 * the existing 2D pipeline. A future revision will move the inner loop to a
 * dedicated full-screen Slang shader.
 *
 * Thread-safety: main thread only.
 */
class OWL_API RendererRaycast {
public:
	RendererRaycast() = default;

	RendererRaycast(const RendererRaycast&) = delete;

	RendererRaycast(RendererRaycast&&) = delete;

	auto operator=(const RendererRaycast&) -> RendererRaycast& = delete;

	auto operator=(RendererRaycast&&) -> RendererRaycast& = delete;

	~RendererRaycast() = default;

	/**
	 * @brief
	 *  Initialise the raycaster facade.
	 *
	 * Currently a no-op since draws are routed through `Renderer2D`. Kept for
	 * symmetry with future GPU-shader-based implementations and to give us a
	 * place to allocate map / atlas SSBOs without breaking call sites.
	 */
	static void init();

	/**
	 * @brief
	 *  Tear down the raycaster facade.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Open a raycast scene.
	 *
	 * Records the camera pose and the configuration that will be used by every
	 * subsequent `draw*` call until `endScene`. The active framebuffer / viewport
	 * size is required to know how many vertical stripes to render and at what
	 * world-units-per-pixel ratio.
	 * @param[in] iCamera The active camera (world transform extracted via inverse view).
	 * @param[in] iViewport The render target dimensions in pixels.
	 * @param[in] iConfig The pass configuration.
	 */
	static void beginScene(const Camera& iCamera, const math::vec2ui& iViewport, const RaycastConfig& iConfig);

	/**
	 * @brief
	 *  Render the walls of a tilemap as DDA stripes plus the sky / floor backdrop.
	 *
	 * Iterates the tilemap's first non-empty layer, treating any cell with a
	 * non-empty tile index as a wall whose texture is sampled from the bound
	 * tileset atlas.
	 * @param[in] iTilemap The tilemap to render. Must have a resolved `tileset`.
	 * @param[in] iTilemapWorldTransform The tilemap's world transform (translation, rotation, scale).
	 *            The caster works in tilemap-local space: camera is transformed accordingly.
	 * @param[in] iEntityId The entity id (written into the picking attachment).
	 */
	static void drawTilemapWalls(const scene::component::Tilemap& iTilemap,
								 const math::Transform& iTilemapWorldTransform, int iEntityId);

	/**
	 * @brief
	 *  Close a raycast scene.
	 *
	 * Flushes the pending stripe quads. Pairs with `beginScene`.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Per-frame statistics.
	 */
	struct OWL_API Statistics {
		/// Number of tilemap calls in the last frame.
		uint32_t drawCalls = 0;
		/// Number of vertical stripes emitted.
		uint32_t stripeCount = 0;
		/// Number of stripes that hit a wall (cells != empty in DDA range).
		uint32_t hitCount = 0;
		/// Number of rays that ran the full max-distance budget without hitting anything.
		uint32_t missCount = 0;
	};

	/**
	 * @brief
	 *  Reset the per-frame statistics.
	 */
	static void resetStats();

	/**
	 * @brief
	 *  Read the current statistics (intended to be called once per frame after `endScene`).
	 * @return The statistics snapshot.
	 */
	[[nodiscard]] static auto getStats() -> Statistics;
};

}// namespace owl::renderer
