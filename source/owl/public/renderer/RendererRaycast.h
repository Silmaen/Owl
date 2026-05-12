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
#include "renderer/gpu/Texture.h"

#include <array>
#include <span>

namespace owl::scene {

class TilemapAsset;
}// namespace owl::scene

namespace owl::math {

class Transform;
}// namespace owl::math

/**
 * @brief
 *  Wolfenstein-style raycaster family.
 *
 * Hosts `RendererRaycast` (static facade running per-column DDA across a
 * `scene::TilemapAsset` to produce a pseudo-3D first-person view)
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
 * grid (a `scene::TilemapAsset`) and synthesises a pseudo-3D first-person
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
 *  Per-pushwall payload consumed by `RendererRaycast::drawDynamicWalls`.
 *
 * A pushwall is a world-space cubic AABB (one cell on each side by default)
 * that can sit at arbitrary sub-cell positions while sliding from its rest
 * position to its final position. The renderer treats each one as an opaque
 * box with the same texture on every face: per screen column it intersects
 * the camera ray with the AABB using the slab method, and if the entry
 * distance is closer than both the static-tilemap wall depth latched in the
 * zBuffer and any other dynamic wall already processed, it emits a textured
 * stripe and updates the zBuffer so the upcoming sprite pass occludes
 * correctly.
 */
struct OWL_API RaycastDynamicWallData {
	/// World-space XY centre of the wall (in cells, matching `Tilemap::cellSize`).
	math::vec2 worldCenter{0.f, 0.f};
	/**
	 * @brief
	 *  Full half-extent of the wall on each axis (cells).
	 *
	 * Default `{0.5, 0.5}` produces a 1-cell cube — the natural shape of a
	 * pushwall. The renderer does not require unit-size AABBs.
	 */
	math::vec2 halfExtent{0.5f, 0.5f};
	/// Texture sampled on every face of the AABB (typically the tileset atlas).
	shared<gpu::Texture> texture;
	/**
	 * @brief
	 *  Sub-rectangle of `texture` to use, in atlas UV space (minU, minV, maxU, maxV).
	 *
	 * The renderer maps each stripe's local `[0,1]` U,V into this sub-rect, so
	 * a pushwall can reference a single tile inside a shared tileset atlas
	 * instead of carrying its own full PNG. Default `{0,0,1,1}` reads the
	 * whole texture.
	 */
	math::vec4 uvRect{0.f, 0.f, 1.f, 1.f};
	/// Colour tint multiplied into the sampled texel (defaults to identity white).
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	/**
	 * @brief
	 *  Vertical scale relative to a 1-cell wall.
	 *
	 * Matches `TileMeta::wallHeight` semantics for static walls: `1.0` is a
	 * standard ceiling-to-floor wall, `>1` extends above the ceiling, `<1`
	 * leaves a gap below the ceiling. Dynamic walls are bottom-anchored at
	 * the floor like static walls.
	 */
	float wallHeight = 1.f;
	/// Entity id written into the framebuffer picking attachment.
	int entityId = -1;
};

/**
 * @brief
 *  Per-door payload consumed by `RendererRaycast::drawDoors`.
 *
 * A door is a 1×1 cell at `cellCenter`. Its visual content is:
 *   - A thin **plate** with zero thickness, perpendicular to `openingDirection`
 *     (the plate's surface normal is **perpendicular** to the opening
 *     direction, so a north/south-opening door has plate surfaces facing
 *     east and west). The plate slides exactly one cell (plus a 1-pixel
 *     margin) into the wall pocket on the opening-direction side; at
 *     `plateOffset = 1.0` the plate is fully hidden.
 *   - Two **laterals** with zero thickness, applied to the two cube inside
 *     faces whose normals are along the slide axis (N and S faces for
 *     N/S opening, E and W faces for E/W opening). They render whenever
 *     a ray's cube-exit face is on those sides — that's the surface the
 *     player sees when looking through the open doorway into the pocket.
 *
 * `faceTexture`'s U=1 edge is always rendered on the opening-direction
 * side of the cell — the renderer flips U appropriately so both sides of
 * the plate display the texture with that orientation.
 */
struct OWL_API RaycastDoorData {
	/**
	 * @brief
	 *  Cardinal direction the plate retracts to.
	 *
	 * Encoded as the same enum as `scene::component::RaycastDoor::OpeningDirection`,
	 * passed as a `uint8_t` to keep the renderer header decoupled from
	 * the scene component header.
	 */
	uint8_t openingDirection = 0;///< 0=N, 1=S, 2=E, 3=W
	/// World-space XY centre of the door's cell.
	math::vec2 cellCenter{0.f, 0.f};
	/// Current plate position along the opening direction, in cells. `0` = closed, `1.0` = fully open.
	float plateOffset = 0.f;
	/// Texture for the moving plate (typically the tileset atlas). U=1 is rendered on the opening-direction side.
	shared<gpu::Texture> faceTexture;
	/// Sub-rectangle of `faceTexture` for the plate tile (minU, minV, maxU, maxV).
	math::vec4 faceUvRect{0.f, 0.f, 1.f, 1.f};
	/// Texture for both lateral inside faces (jamb / track).
	shared<gpu::Texture> lateralTexture;
	/// Sub-rectangle of `lateralTexture` for the lateral tile (minU, minV, maxU, maxV).
	math::vec4 lateralUvRect{0.f, 0.f, 1.f, 1.f};
	/// Colour tint multiplied into the sampled texel.
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	/// Vertical scale relative to a 1-cell wall (same semantics as static walls).
	float wallHeight = 1.f;
	/// Entity id written into the framebuffer picking attachment.
	int entityId = -1;
};

/**
 * @brief
 *  Per-sprite payload consumed by `RendererRaycast::drawSprites`.
 *
 * Sprites are billboards: their world XY position is projected through the
 * camera basis the same way wall stripes are, then stretched / centred on the
 * screen according to the sprite's world-size and the per-column zBuffer
 * latched by `drawTilemapWalls`.
 */
struct OWL_API RaycastSpriteData {
	/// World-space XY position of the sprite centre (cells, same units as `Tilemap::cellSize`).
	math::vec2 worldPosition{0.f, 0.f};
	/**
	 * @brief
	 *  Vertical world offset (in cells) shifting the sprite's screen centre away from the horizon.
	 *
	 * `0` keeps the sprite vertically centred on the horizon — i.e. it spans the
	 * floor-to-ceiling band like a `worldSize.y == 1` wall would. Positive values
	 * raise the sprite (hanging lamps, ceiling-mounted lights), negative values
	 * lower it (floor-tile decals).
	 */
	float worldZOffset = 0.f;
	/**
	 * @brief
	 *  World-space size (width × height in cells).
	 *
	 * A sprite of size `{1, 1}` reaches the same screen height as a 1-cell-tall
	 * wall at the same depth — so authoring scales line up with the tilemap grid.
	 */
	math::vec2 worldSize{1.f, 1.f};
	/// Colour tint multiplied into the sampled texel.
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	/// Sprite texture (sprites without a texture are silently skipped).
	shared<gpu::Texture> texture;
	/**
	 * @brief
	 *  Per-vertex texture coordinates, ordered BL → BR → TR → TL (matches `Quad2DData`).
	 *
	 * Defaults to the full atlas. Use sub-rectangles to render frames out of a
	 * spritesheet (animated sprites).
	 */
	std::array<math::vec2, 4> textureCoords{math::vec2{0.f, 0.f}, math::vec2{1.f, 0.f}, math::vec2{1.f, 1.f},
											math::vec2{0.f, 1.f}};
	/// Entity id written into the framebuffer picking attachment.
	int entityId = -1;
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
	 * @param[in] iTilemap The tilemap asset (grid + tileset) to render. Must have a resolved
	 *            `tileset`.
	 * @param[in] iTilemapWorldTransform The tilemap entity's world transform (translation,
	 *            rotation, scale). The caster works in tilemap-local space: camera is
	 *            transformed accordingly.
	 * @param[in] iEntityId The entity id (written into the picking attachment).
	 */
	static void drawTilemapWalls(const scene::TilemapAsset& iTilemap, const math::Transform& iTilemapWorldTransform,
								 int iEntityId);

	/**
	 * @brief
	 *  Render a batch of pushwalls on top of the static tilemap pass.
	 *
	 * Each pushwall is a world-space AABB processed independently per
	 * screen column via slab-method ray/AABB intersection. The renderer
	 * keeps the closest opaque depth per column in the same zBuffer that
	 * `drawTilemapWalls` populated, so sprites drawn afterwards are
	 * occluded by pushwalls just like by static walls. Walls with no
	 * texture are silently skipped.
	 *
	 * Must be called between `beginScene` and `endScene`, after
	 * `drawTilemapWalls` (the dynamic pass refines the static zBuffer)
	 * and before `drawSprites`. An empty span is a no-op.
	 * @param[in] iWalls Pushwall payloads to render.
	 */
	static void drawDynamicWalls(std::span<const RaycastDynamicWallData> iWalls);

	/**
	 * @brief
	 *  Render a batch of doors on top of the static tilemap pass.
	 *
	 * Each door is rendered as three independent AABBs: two static lateral
	 * slabs (cell sides parallel to `slideDirection`, carrying
	 * `lateralTexture`) and one moving plate slab (perpendicular to
	 * `slideDirection`, positioned at the cell centre when closed,
	 * translated by `plateOffset` toward the pocket when opening, carrying
	 * `faceTexture`). For each screen column the renderer picks the closest
	 * sub-AABB that beats the current zBuffer and emits one textured
	 * stripe. The zBuffer is refined per stripe so sprites and farther
	 * walls occlude correctly.
	 *
	 * Must be called between `beginScene` and `endScene`, after
	 * `drawTilemapWalls` / `drawDynamicWalls` and before `drawSprites`.
	 * An empty span is a no-op.
	 * @param[in] iDoors Door payloads to render.
	 */
	static void drawDoors(std::span<const RaycastDoorData> iDoors);

	/**
	 * @brief
	 *  Render a batch of billboard sprites in the active raycast scene.
	 *
	 * The facade takes ownership of:
	 *  - **Camera-space projection**: each sprite's world XY is transformed into
	 *    `(transformX, transformY)` using the camera basis (forward + plane);
	 *    sprites with `transformY <= 0` (behind the camera) or beyond
	 *    `RaycastConfig::maxDistance` are culled.
	 *  - **Back-to-front sort**: visible sprites are drawn farthest first so
	 *    nearer sprites correctly overdraw farther ones.
	 *  - **Per-column wall occlusion**: the sprite is split into 1-pixel-wide
	 *    column strips; for each column the wall depth latched by
	 *    `drawTilemapWalls` is compared against the sprite depth, hidden columns
	 *    are skipped.
	 *
	 * Must be called between `beginScene` and `endScene`. An empty span is a
	 * silent no-op.
	 * @param[in] iSprites Sprite payloads to render.
	 */
	static void drawSprites(std::span<const RaycastSpriteData> iSprites);

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
		/// Number of sprites that survived camera-space culling and contributed at least one stripe.
		uint32_t spriteCount = 0;
		/// Number of 1-pixel-wide stripes emitted for sprites this frame.
		uint32_t spriteStripeCount = 0;
		/// Number of sprite stripes that were skipped because a wall sat in front of them.
		uint32_t spriteOccludedCount = 0;
		/// Number of pushwalls that contributed at least one stripe.
		uint32_t dynamicWallCount = 0;
		/// Number of 1-pixel-wide stripes emitted for pushwalls this frame.
		uint32_t dynamicWallStripeCount = 0;
		/// Number of doors that contributed at least one stripe (lateral or plate).
		uint32_t doorCount = 0;
		/// Number of 1-pixel-wide stripes emitted for doors this frame.
		uint32_t doorStripeCount = 0;
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
