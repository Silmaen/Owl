/**
 * @file RendererIsometric.h
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/Transform.h"
#include "math/matrices.h"
#include "math/vectors.h"
#include "renderer/Camera.h"
#include "renderer/CameraOrtho.h"
#include "renderer/IsometricObjects.h"
#include "renderer/gpu/Texture.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace owl::scene {
class TilemapAsset;
}// namespace owl::scene

namespace owl::renderer {

/**
 * @brief
 *  Shape of the 2:1 dimetric presentation.
 *
 * World units are tiles: `x` runs towards the lower right of the screen, `y` towards the lower left, `z` up.
 */
struct OWL_API IsometricConfig {
	/// Tile sprite footprint in pixels; the X:Y ratio sets the pitch (default 64x32 = 2:1, a 30° pitch).
	math::vec2ui tileSize{64u, 32u};
	/// Vertical pixels added per world Z unit (elevation step for stacked tiles / buildings).
	float zStep = 16.f;
	/// Screen-space pixel offset where world origin (0, 0, 0) projects (used by `worldToScreen` / `screenToWorld`).
	math::vec2 origin{0.f, 0.f};
};

/**
 * @brief
 *  Project a world point to isometric screen pixels (Y down).
 *
 * `screen.x = origin.x + (x - y) * tileW / 2`, `screen.y = origin.y + (x + y) * tileH / 2 - z * zStep`.
 * @param[in] iWorld World point, in tiles.
 * @param[in] iConfig Tile size, height step and screen origin.
 * @return The screen point, in pixels.
 */
[[nodiscard]] OWL_API auto worldToScreen(const math::vec3& iWorld, const IsometricConfig& iConfig) -> math::vec2;

/**
 * @brief
 *  Back-project screen pixels (Y down) onto the horizontal plane at a given height.
 * @param[in] iScreen Screen point, in pixels.
 * @param[in] iWorldZ Height of the plane, in world units.
 * @param[in] iConfig Tile size, height step and screen origin.
 * @return The world point on the plane `z = iWorldZ`.
 */
[[nodiscard]] OWL_API auto screenToWorld(const math::vec2& iScreen, float iWorldZ, const IsometricConfig& iConfig)
		-> math::vec3;

/**
 * @brief
 *  Pitch of the dimetric view: the angle whose sine is the tile height over its width (30° for 2:1).
 * @param[in] iConfig The presentation.
 * @return The pitch, in radians.
 */
[[nodiscard]] OWL_API auto getIsometricPitch(const IsometricConfig& iConfig) -> float;

/**
 * @brief
 *  Isometric camera: a real view (45° yaw, pitch from the tile ratio, Z scaled by `zStep`) times a `CameraOrtho`
 *  projection in pixels.
 *
 * The view maps world tiles to view pixels: `x` right, `y` up, `z` towards the viewer (a larger view depth is
 * nearer). World Y is mirrored so the grid keeps the usual layout (`x` to the lower right, `y` to the lower left);
 * distances and depth are unchanged. The projection is centred on the target, scaled by the zoom (a power of two),
 * with the Vulkan Y flip and depth range of `CameraOrtho`.
 */
class OWL_API IsometricCamera final : public Camera {
public:
	/**
	 * @brief
	 *  Copy constructor.
	 */
	IsometricCamera(const IsometricCamera&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	IsometricCamera(IsometricCamera&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const IsometricCamera&) -> IsometricCamera& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(IsometricCamera&&) -> IsometricCamera& = default;

	/**
	 * @brief
	 *  Camera with the default 64x32 presentation, looking at the origin, zoom 1, 1280x720 viewport.
	 */
	IsometricCamera();

	/**
	 * @brief
	 *  Camera with a given presentation.
	 * @param[in] iConfig The presentation.
	 */
	explicit IsometricCamera(const IsometricConfig& iConfig);

	/**
	 * @brief
	 *  Destructor.
	 */
	~IsometricCamera() override;

	/// Smallest zoom level (zoom = 2^level, a sixteenth).
	static constexpr int32_t minZoomLevel = -4;
	/// Largest zoom level (zoom = 2^level, sixteen times).
	static constexpr int32_t maxZoomLevel = 4;
	/// Half depth of the view volume around the target, in view pixels.
	static constexpr float depthRange = 32768.f;

	/**
	 * @brief
	 *  Set the presentation (tile size, height step).
	 * @param[in] iConfig The presentation.
	 */
	void setConfig(const IsometricConfig& iConfig);

	/**
	 * @brief
	 *  Get the presentation.
	 * @return The presentation.
	 */
	[[nodiscard]] auto getConfig() const -> const IsometricConfig& { return m_config; }

	/**
	 * @brief
	 *  Set the world point shown at the centre of the viewport.
	 * @param[in] iTarget The target, in tiles.
	 */
	void setTarget(const math::vec3& iTarget);

	/**
	 * @brief
	 *  Get the world point shown at the centre of the viewport.
	 * @return The target, in tiles.
	 */
	[[nodiscard]] auto getTarget() const -> const math::vec3& { return m_target; }

	/**
	 * @brief
	 *  Set the zoom level, clamped to [`minZoomLevel`, `maxZoomLevel`].
	 * @param[in] iLevel The level: the zoom is `2^level`.
	 */
	void setZoomLevel(int32_t iLevel);

	/**
	 * @brief
	 *  Get the zoom level.
	 * @return The level: the zoom is `2^level`.
	 */
	[[nodiscard]] auto getZoomLevel() const -> int32_t { return m_zoomLevel; }

	/**
	 * @brief
	 *  Get the zoom factor: screen pixels per sprite pixel.
	 * @return `2^level`.
	 */
	[[nodiscard]] auto getZoom() const -> float;

	/**
	 * @brief
	 *  Set the viewport size.
	 * @param[in] iSize The size, in pixels.
	 */
	void setViewportSize(const math::vec2ui& iSize);

	/**
	 * @brief
	 *  Get the viewport size.
	 * @return The size, in pixels.
	 */
	[[nodiscard]] auto getViewportSize() const -> const math::vec2ui& { return m_viewportSize; }

	/**
	 * @brief
	 *  Move the target so the content follows a drag of the cursor.
	 * @param[in] iPixelDelta The cursor motion, in viewport pixels (Y down).
	 */
	void pan(const math::vec2& iPixelDelta);

	/**
	 * @brief
	 *  Project a world point into the viewport.
	 * @param[in] iWorld World point, in tiles.
	 * @return The viewport pixel, origin at the top left, Y down.
	 */
	[[nodiscard]] auto worldToViewport(const math::vec3& iWorld) const -> math::vec2;

	/**
	 * @brief
	 *  Back-project a viewport pixel onto the horizontal plane at a given height.
	 * @param[in] iPixel Viewport pixel, origin at the top left, Y down.
	 * @param[in] iWorldZ Height of the plane.
	 * @return The world point on the plane `z = iWorldZ`.
	 */
	[[nodiscard]] auto viewportToWorld(const math::vec2& iPixel, float iWorldZ) const -> math::vec3;

	/**
	 * @brief
	 *  Depth of a world point along the view axis.
	 * @param[in] iWorld World point, in tiles.
	 * @return The view depth, in view pixels: larger is nearer the viewer.
	 */
	[[nodiscard]] auto getViewDepth(const math::vec3& iWorld) const -> float;

	/**
	 * @brief
	 *  Build the view matrix of a presentation looking at a target.
	 * @param[in] iConfig The presentation.
	 * @param[in] iTarget The target, in tiles.
	 * @return The view matrix (world tiles to view pixels).
	 */
	[[nodiscard]] static auto makeView(const IsometricConfig& iConfig, const math::vec3& iTarget) -> math::mat4;

	/**
	 * @brief
	 *  Build the projection matrix of a viewport at a zoom, in a clip-space convention.
	 * @param[in] iViewport The viewport size, in pixels.
	 * @param[in] iZoom The zoom factor.
	 * @param[in] iClipSpace The convention of the target backend.
	 * @return The projection matrix (view pixels to clip space).
	 */
	[[nodiscard]] static auto makeProjection(const math::vec2ui& iViewport, float iZoom,
											 CameraOrtho::ClipSpace iClipSpace) -> math::mat4;

private:
	/**
	 * @brief
	 *  Rebuild the view, the projection and their product.
	 */
	void recalculate();

	/// Tile size and height step.
	IsometricConfig m_config;
	/// World point at the centre of the viewport.
	math::vec3 m_target{0.f, 0.f, 0.f};
	/// Zoom level (zoom = 2^level).
	int32_t m_zoomLevel = 0;
	/// Viewport size in pixels.
	math::vec2ui m_viewportSize{1280u, 720u};
};

/**
 * @brief
 *  A pre-rendered sprite drawn by the isometric renderer, anchored at its world foot point.
 *
 * The sprite keeps its pixels (scaled by the zoom only): its bottom centre sits on the projected foot point. An
 * upright billboard stands as a vertical plane facing the viewer, a flat one (a terrain tile) lies on the ground
 * plane through the foot; each pixel writes the depth of that plane. A tint alpha below 1 makes it translucent.
 */
struct OWL_API IsometricBillboard {
	/// World foot point, in tiles.
	math::vec3 foot{0.f, 0.f, 0.f};
	/// Sprite size at zoom 1, in pixels.
	math::vec2 size{64.f, 64.f};
	/// Texture coordinates of the corners: bottom left, bottom right, top right, top left.
	std::array<math::vec2, 4> textureCoords{math::vec2{0.f, 0.f}, math::vec2{1.f, 0.f}, math::vec2{1.f, 1.f},
											math::vec2{0.f, 1.f}};
	/// Tint; an alpha below 1 sends the sprite to the sorted translucent pass.
	math::vec4 color{1.f, 1.f, 1.f, 1.f};
	/// Texture (white when null).
	shared<gpu::Texture2D> texture = nullptr;
	/// Depth offset towards the viewer, in view pixels (sub-layer order of sprites on the same plane).
	float depthBias = 0.f;
	/// Lie on the ground plane (terrain tile) instead of standing upright.
	bool flat = false;
	/// Entity id written to the picking attachment.
	int entityId = -1;
};

/**
 * @brief
 *  A compact sprite instance: an anchor on the ground, a size and a texture rectangle, no matrix.
 *
 * 64 bytes on the GPU against 96 for an `IsometricBillboard`: made for thousands of moving sprites (vehicles) and
 * for the slices of buildings. Always upright, opaque or cut out (a texel whose alpha times the tint alpha is below
 * one half is discarded), drawn in the depth-tested pass in submission order. The foot is snapped to whole screen
 * pixels at an integer zoom.
 */
struct OWL_API IsometricSprite {
	/// World foot point, in tiles.
	math::vec3 foot{0.f, 0.f, 0.f};
	/// Sprite size at zoom 1, in pixels.
	math::vec2 size{64.f, 64.f};
	/// Point of the sprite put on the foot, as a fraction of its size from the bottom left (bottom centre by default).
	math::vec2 anchor{0.5f, 0.f};
	/// Texture rectangle: u of the left, v of the bottom, u of the right, v of the top (swap to flip).
	math::vec4 uvRect{0.f, 0.f, 1.f, 1.f};
	/// Tint, RGBA8 with red in the low byte (`packColor`).
	uint32_t color = 0xFFFFFFFFu;
	/// Depth offset towards the viewer, in view pixels.
	float depthBias = 0.f;
	/// Entity id written to the picking attachment.
	int entityId = -1;
};

/**
 * @brief
 *  Pack a colour into the RGBA8 of an `IsometricSprite`.
 * @param[in] iColor Colour in [0, 1] per channel (clamped).
 * @return The packed colour, red in the low byte.
 */
[[nodiscard]] OWL_API auto packColor(const math::vec4& iColor) -> uint32_t;

/**
 * @brief
 *  Screen rectangle covered by a sprite, in viewport pixels (Y down).
 * @param[in] iCamera The camera of the viewport.
 * @param[in] iSprite The sprite.
 * @return The rectangle: x min, y min, x max, y max.
 */
[[nodiscard]] OWL_API auto getSpriteScreenRect(const IsometricCamera& iCamera, const IsometricSprite& iSprite)
		-> math::vec4;

/**
 * @brief
 *  Order in which translucent billboards are drawn: back to front.
 *
 * Farther foot points first; at equal depth, the smaller depth bias first; then the submission order.
 * @param[in] iCamera The camera giving the view depth.
 * @param[in] iBillboards The billboards.
 * @return Indices into `iBillboards`, in draw order.
 */
[[nodiscard]] OWL_API auto getBackToFrontOrder(const IsometricCamera& iCamera,
											   std::span<const IsometricBillboard> iBillboards)
		-> std::vector<uint32_t>;

/**
 * @brief
 *  Tile of an isometric tilemap under a viewport pixel.
 *
 * Cell `(cx, cy)` covers the world square from `origin + (cx, cy)` to `origin + (cx + 1, cy + 1)`, at the height of
 * the origin.
 * @param[in] iCamera The camera of the viewport.
 * @param[in] iPixel Viewport pixel, origin at the top left, Y down.
 * @param[in] iMapOrigin World position of the map's back corner (the entity translation).
 * @param[in] iMapSize Map size in cells.
 * @return The cell under the pixel, or nothing when the pixel is off the map.
 */
[[nodiscard]] OWL_API auto pickIsometricCell(const IsometricCamera& iCamera, const math::vec2& iPixel,
											 const math::vec3& iMapOrigin, const math::vec2ui& iMapSize)
		-> std::optional<math::vec2ui>;

/**
 * @brief
 *  Static renderer of the isometric layer: depth-tested billboards with alpha cutout.
 *
 * Opaque and cut-out billboards (texel alpha below one half is discarded) draw in submission order with depth test
 * and write; translucent ones draw after them, sorted back to front, depth tested but not written.
 *
 * A tilemap keeps one persistent GPU instance buffer per chunk of 32x32 cells and per layer (the 2D tilemap scheme):
 * `prepareTilemap` (the prepare phase, before any render pass) rebuilds and uploads only the chunks whose cell
 * generation, height generation or tileset changed, and a frame draws one instanced call per chunk in view without
 * uploading anything. A chunk first met at draw time is built there (counted as a late build).
 */
class OWL_API RendererIsometric final {
public:
	/**
	 * @brief
	 *  Default constructor (deleted: static API).
	 */
	RendererIsometric() = delete;

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	RendererIsometric(const RendererIsometric&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	RendererIsometric(RendererIsometric&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return A reference to this object.
	 */
	auto operator=(const RendererIsometric&) -> RendererIsometric& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return A reference to this object.
	 */
	auto operator=(RendererIsometric&&) -> RendererIsometric& = delete;

	/**
	 * @brief
	 *  Destructor (deleted).
	 */
	~RendererIsometric() = delete;

	/// Billboards per GPU batch (a full batch is flushed and a new one starts).
	static constexpr uint32_t maxBillboardsPerBatch = 20000;
	/// Compact sprites per GPU batch.
	static constexpr uint32_t maxSpritesPerBatch = 40000;
	/// Depth offset between two layers of a tilemap, in view pixels.
	static constexpr float tilemapLayerBias = 0.0625f;
	/// Depth offset of upright sprites, above every tilemap layer of the same plane, in view pixels.
	static constexpr float spriteBias = 1.f;
	/// Prepare phases an unused tilemap stays cached before `beginPrepare` evicts it.
	static constexpr uint64_t cacheEvictionPrepares = 600;

	/**
	 * @brief
	 *  Create the descriptor block, the buffers and the pipelines.
	 */
	static void init();

	/**
	 * @brief
	 *  Release the GPU resources.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Start a prepare phase: reset the prepare counters and evict the tilemaps unused for a long time.
	 */
	static void beginPrepare();

	/**
	 * @brief
	 *  Bring the GPU chunks of a tilemap up to date; call it outside any render pass.
	 *
	 * Only the chunks whose generation changed (cells, corner heights, or any change of the tileset) are rebuilt and
	 * uploaded, so a frame without edit uploads nothing. The chunks do not depend on the camera, the transform or
	 * the presentation.
	 * @param[in] iAsset The tilemap.
	 */
	static void prepareTilemap(const scene::TilemapAsset& iAsset);

	/**
	 * @brief
	 *  Drop every cached tilemap.
	 */
	static void clearCache();

	/**
	 * @brief
	 *  Number of tilemaps with GPU chunks.
	 * @return The count.
	 */
	[[nodiscard]] static auto getCachedAssetCount() -> size_t;

	/**
	 * @brief
	 *  Start a frame of the layer.
	 * @param[in] iCamera The isometric camera.
	 */
	static void beginScene(const IsometricCamera& iCamera);

	/**
	 * @brief
	 *  Queue a billboard.
	 * @param[in] iBillboard The billboard.
	 */
	static void drawBillboard(const IsometricBillboard& iBillboard);

	/**
	 * @brief
	 *  Queue one compact sprite; one outside the viewport is dropped.
	 * @param[in] iTexture Texture (white when null).
	 * @param[in] iSprite The sprite.
	 */
	static void drawSprite(const shared<gpu::Texture2D>& iTexture, const IsometricSprite& iSprite);

	/**
	 * @brief
	 *  Queue many compact sprites sharing one texture (a vehicle atlas); those outside the viewport are dropped.
	 * @param[in] iTexture Texture (white when null).
	 * @param[in] iSprites The sprites.
	 */
	static void drawSprites(const shared<gpu::Texture2D>& iTexture, std::span<const IsometricSprite> iSprites);

	/**
	 * @brief
	 *  Queue the slices of an object of a sheet.
	 *
	 * Each slice is an upright sprite anchored at the front corner of its footprint tile, so every slice tests its
	 * own depth; a slice outside the viewport is dropped like any sprite.
	 * @param[in] iSet The sheet.
	 * @param[in] iObject The object, from `iSet`.
	 * @param[in] iOrigin World position of the back corner of the object's footprint.
	 * @param[in] iColor Tint.
	 * @param[in] iEntityId Entity id written to the picking attachment.
	 */
	static void drawObject(const IsometricObjectSet& iSet, const IsometricObjectDef& iObject, const math::vec3& iOrigin,
						   const math::vec4& iColor, int iEntityId);

	/**
	 * @brief
	 *  Queue an isometric tilemap: its chunks in view are drawn from their persistent GPU buffers.
	 *
	 * The map's back corner sits at the entity translation; rotation and scale are ignored. Each layer is one depth
	 * sub-layer above the previous one (`tilemapLayerBias`), its overlays half a sub-layer above its terrain.
	 * @param[in] iAsset The tilemap.
	 * @param[in] iWorldTransform World transform of the entity.
	 * @param[in] iEntityId Entity id written to the picking attachment.
	 */
	static void drawTilemap(const scene::TilemapAsset& iAsset, const math::Transform& iWorldTransform, int iEntityId);

	/**
	 * @brief
	 *  Draw the queued billboards: the opaque pass, then the sorted translucent pass.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Counters of the last frame.
	 */
	struct Statistics {
		/// Draw calls issued.
		uint32_t drawCallCount = 0;
		/// Opaque and cut-out billboards and tilemap cells drawn.
		uint32_t opaqueCount = 0;
		/// Translucent billboards drawn.
		uint32_t translucentCount = 0;
		/// Tilemap chunks skipped because they lie outside the view.
		uint32_t culledChunkCount = 0;
		/// Compact sprites drawn.
		uint32_t spriteCount = 0;
		/// Compact sprites dropped because they lie outside the view.
		uint32_t culledSpriteCount = 0;
	};

	/**
	 * @brief
	 *  Counters of the chunk uploads since the last `beginPrepare`.
	 */
	struct PrepareStatistics {
		/// Chunks built and uploaded.
		uint32_t builtChunkCount = 0;
		/// Chunks built at draw time because no prepare ran for them (uploads inside the render pass).
		uint32_t lateBuiltChunkCount = 0;
		/// Bytes sent to the GPU buffers of the chunks.
		uint64_t uploadedBytes = 0;
	};

	/**
	 * @brief
	 *  Get the counters since the last `resetStats`.
	 * @return The counters.
	 */
	[[nodiscard]] static auto getStatistics() -> Statistics;

	/**
	 * @brief
	 *  Get the upload counters since the last `beginPrepare`.
	 * @return The counters.
	 */
	[[nodiscard]] static auto getPrepareStatistics() -> PrepareStatistics;

	/**
	 * @brief
	 *  Reset the counters.
	 */
	static void resetStats();
};

}// namespace owl::renderer
