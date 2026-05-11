/**
 * @file RendererRaycast.cpp
 * @author Silmaen
 * @date 04/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererRaycast.h"

#include "math/Transform.h"
#include "math/trigonometry.h"
#include "renderer/CameraOrtho.h"
#include "renderer/Renderer2D.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/component/Tilemap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

namespace owl::renderer {

namespace {
/// Per-pass state shared between `beginScene`, the `drawTilemap*` calls, and `endScene`.
struct State {
	/// Camera world position (XY plane).
	math::vec2 cameraPos2D{0.f, 0.f};
	/// Camera forward direction (XY plane, unit length).
	math::vec2 cameraDir2D{1.f, 0.f};
	/// Camera right direction times tan(fov/2) — encodes both FOV and aspect.
	math::vec2 cameraPlane2D{0.f, 1.f};
	/// Render target dimensions in pixels.
	math::vec2ui viewport{1, 1};
	/// Current pass configuration.
	RaycastConfig config;
	/// Whether `beginScene` has been called without a matching `endScene`.
	bool sceneOpen = false;
	/**
	 * Whether the sky / floor backdrop has already been emitted for this scene.
	 * Drawn lazily on the first `drawTilemapWalls` so passes that route no
	 * tilemap stay genuinely no-op (no overdraw on top of a 2D layer underneath).
	 */
	bool backdropEmitted = false;
	/**
	 * Per-column wall depth (in cell units), latched by `drawTilemapWalls` and
	 * read by `drawSprites` so billboards can be occluded by walls. Sized to
	 * the active `numRays` at `beginScene`; entries default to `+inf` (no
	 * wall hit on that column).
	 */
	std::vector<float> zBufferPerColumn;
	/// Cumulative statistics for the current frame (cleared by `resetStats`).
	RendererRaycast::Statistics stats;
};

shared<State> g_state;

/// Side darkening factor applied to walls hit on a Y-cell-edge (gives a cheap "lighting" cue).
constexpr float g_YSideDarken = 0.7f;

/// Numerical floor for perpendicular-distance to avoid division by zero on grazing rays.
constexpr float g_MinPerpDist = 1e-4f;

/// Squared-length floor used when picking a fallback for degenerate camera vectors.
constexpr float g_DirEpsilonSq = 1e-8f;

/**
 * @brief
 *  Resolve the "active" tilemap layer index for raycasting.
 *
 * The raycaster treats walls as 2D occupancy: a non-empty cell is a wall, regardless of which
 * `TilemapLayer` it lives on. v0.2.0 picks the first visible layer with any tile data; future
 * revisions can walk every layer to support thin walls / variable heights.
 * @return Layer index, or `iTilemap.layers.size()` if no suitable layer was found.
 */
auto pickActiveLayerIndex(const scene::TilemapAsset& iTilemap) -> size_t {
	for (size_t i = 0; i < iTilemap.layers.size(); ++i) {
		const auto& layer = iTilemap.layers[i];
		if (!layer.visible)
			continue;
		if (layer.tiles.empty())
			continue;
		return i;
	}
	return iTilemap.layers.size();
}

}// namespace

void RendererRaycast::init() {
	if (g_state) {
		OWL_CORE_WARN("RendererRaycast already initiated.")
		g_state.reset();
	}
	g_state = mkShared<State>();
}

void RendererRaycast::shutdown() { g_state.reset(); }

void RendererRaycast::beginScene(const Camera& iCamera, const math::vec2ui& iViewport, const RaycastConfig& iConfig) {
	OWL_PROFILE_FUNCTION()

	if (!g_state) {
		OWL_CORE_ERROR("RendererRaycast::beginScene called before init.")
		return;
	}
	g_state->viewport = (iViewport.x() == 0 || iViewport.y() == 0) ? math::vec2ui{1, 1} : iViewport;
	g_state->config = iConfig;

	// 2D pose extracted from the inverse of the view matrix:
	//   pos    = inverseView * (0,0,0,1)    — world origin of the camera
	//   fwd    = inverseView * (0,1,0,0)    — *local +Y* mapped to world XY plane
	//   right  = inverseView * (1,0,0,0)    — local +X mapped to world XY plane
	//
	// Convention: a 2D `CameraOrtho` with `rotation = 0` has identity inverseView, so
	// `fwd = (0, 1)` — the player faces world +Y. Increasing the camera rotation
	// (degrees, see `CameraOrtho::setRotation`) turns the facing clockwise. The
	// `-Z` axis of standard 3D Owl conventions is **not** used here because 2D
	// ortho cameras are azimuthally invariant around -Z and would always extract
	// `(0, 0)` as their 2D forward.
	const math::mat4 invView = inverse(iCamera.getView());
	const math::vec4 worldPos = invView * math::vec4{0.f, 0.f, 0.f, 1.f};
	const math::vec4 worldFwd = invView * math::vec4{0.f, 1.f, 0.f, 0.f};
	const math::vec4 worldRight = invView * math::vec4{1.f, 0.f, 0.f, 0.f};

	g_state->cameraPos2D = math::vec2{worldPos.x(), worldPos.y()};

	// `Vector::normalize()` leaves a near-zero vector unchanged; we still want a
	// usable fallback in that case, so substitute *before* normalising.
	math::vec2 dir2D{worldFwd.x(), worldFwd.y()};
	if (dir2D.normSq() < g_DirEpsilonSq)
		dir2D = math::vec2{0.f, 1.f};
	g_state->cameraDir2D = dir2D.normalized();

	math::vec2 right2D{worldRight.x(), worldRight.y()};
	if (right2D.normSq() < g_DirEpsilonSq)
		right2D = math::vec2{1.f, 0.f};
	right2D.normalize();

	// Camera plane = right · tan(fov/2). Aspect is intentionally NOT folded into the
	// plane: the `fovDegrees` config knob is the **horizontal** FOV at the actual
	// viewport aspect, mirroring the classic Wolfenstein convention (lodev's
	// raycaster keeps `plane = 0.66` regardless of resolution → ~66° horizontal FOV).
	// Folding aspect in here would over-stretch the cone on wide displays.
	const float halfFovTan = std::tan(math::radians(iConfig.fovDegrees) * 0.5f);
	g_state->cameraPlane2D = right2D * halfFovTan;

	g_state->sceneOpen = true;
	g_state->backdropEmitted = false;
	const uint32_t numRaysActive = (iConfig.numRays > 0) ? iConfig.numRays : g_state->viewport.x();
	g_state->zBufferPerColumn.assign(numRaysActive, std::numeric_limits<float>::infinity());
}

namespace {
auto computeHorizonY() -> float {
	if (!g_state)
		return 0.f;
	return std::floor(static_cast<float>(g_state->viewport.y()) * 0.5f);
}

void emitBackdropIfNeeded() {
	if (!g_state || g_state->backdropEmitted)
		return;
	const math::vec2 vp = {static_cast<float>(g_state->viewport.x()), static_cast<float>(g_state->viewport.y())};
	const float horizonY = computeHorizonY();
	const float skyHeight = vp.y() - horizonY;
	const float floorHeight = horizonY;

	math::Transform skyTransform;
	skyTransform.translation() = math::vec3{vp.x() * 0.5f, horizonY + skyHeight * 0.5f, 0.f};
	skyTransform.scale() = math::vec3{vp.x(), skyHeight, 1.f};
	Renderer2D::drawQuad({.transform = skyTransform, .color = g_state->config.ceilingColor});

	math::Transform floorTransform;
	floorTransform.translation() = math::vec3{vp.x() * 0.5f, floorHeight * 0.5f, 0.f};
	floorTransform.scale() = math::vec3{vp.x(), floorHeight, 1.f};
	Renderer2D::drawQuad({.transform = floorTransform, .color = g_state->config.floorColor});

	g_state->backdropEmitted = true;
}

}// namespace

void RendererRaycast::drawTilemapWalls(const scene::TilemapAsset& iTilemap,
									   [[maybe_unused]] const math::Transform& iTilemapWorldTransform,
									   const int iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_state || !g_state->sceneOpen) {
		OWL_CORE_WARN("RendererRaycast::drawTilemapWalls called outside beginScene/endScene.")
		return;
	}
	// Log skip reasons once per scene activation (the dispatch is per-frame so we
	// don't want to spam the log every frame).
	static bool s_warnedNoTileset = false;
	static bool s_warnedNoTexture = false;
	static bool s_warnedNoLayer = false;
	if (!iTilemap.tileset) {
		if (!s_warnedNoTileset) {
			OWL_CORE_WARN("RendererRaycast: tilemap has no tileset assigned — walls skipped.")
			s_warnedNoTileset = true;
		}
		return;
	}
	if (!iTilemap.tileset->texture) {
		if (!s_warnedNoTexture) {
			OWL_CORE_WARN(
					"RendererRaycast: tilemap tileset has no texture — walls skipped (asset path resolution failed?).")
			s_warnedNoTexture = true;
		}
		return;
	}
	if (iTilemap.width == 0 || iTilemap.height == 0)
		return;
	const size_t layerIdx = pickActiveLayerIndex(iTilemap);
	if (layerIdx >= iTilemap.layers.size()) {
		if (!s_warnedNoLayer) {
			OWL_CORE_WARN("RendererRaycast: tilemap has no visible non-empty layer — walls skipped.")
			s_warnedNoLayer = true;
		}
		return;
	}
	// Reset diagnostic latches once everything is OK so subsequent reload-and-fail
	// scenarios still produce a visible warning.
	s_warnedNoTileset = false;
	s_warnedNoTexture = false;
	s_warnedNoLayer = false;

	g_state->stats.drawCalls++;

	emitBackdropIfNeeded();

	const float cellSize = (iTilemap.cellSize > 0.f) ? iTilemap.cellSize : 1.f;
	const float invCellSize = 1.f / cellSize;
	// Tilemap is rendered centred at world origin. The 2D path centres each cell `c` at
	// world `(c - (W-1)/2) * cellSize`, so cell `c` occupies world X `[(c - W/2) * cellSize,
	// (c + 1 - W/2) * cellSize)` and similar on Y. DDA wants a coordinate space where cell
	// `c` occupies cellCoord `[c, c+1)` — so the half-extent of the conversion is W/2 (not
	// (W-1)/2 — that bug shifted the camera by half a cell in v0.2.0's first cut).
	const float halfW = static_cast<float>(iTilemap.width) * 0.5f;
	const float halfH = static_cast<float>(iTilemap.height) * 0.5f;
	// Convert camera world pos → cell coords. Y is flipped because cell-Y grows downward
	// while world-Y grows upward.
	const math::vec2 camCellPos{
			g_state->cameraPos2D.x() * invCellSize + halfW,
			halfH - g_state->cameraPos2D.y() * invCellSize,
	};
	// Likewise, the ray direction's Y component is negated when expressed in cell space.
	const math::vec2 camCellDir{g_state->cameraDir2D.x(), -g_state->cameraDir2D.y()};
	const math::vec2 camCellPlane{g_state->cameraPlane2D.x(), -g_state->cameraPlane2D.y()};
	const uint32_t numRays = (g_state->config.numRays > 0) ? g_state->config.numRays : g_state->viewport.x();
	if (numRays == 0)
		return;
	const math::vec2 vp = {static_cast<float>(g_state->viewport.x()), static_cast<float>(g_state->viewport.y())};
	// Stripe screen width — exactly viewport width / numRays, with no overlap. The
	// previous 1.5% overlap was meant to hide seams but introduced sub-pixel double-
	// coverage at every boundary; under camera motion that flickered visibly because
	// the GPU's top-left rule alternates which neighbour wins per frame. With
	// `numRays = viewportWidth` (the default) each stripe is exactly 1 pixel wide
	// and adjacent stripes tile seamlessly under the standard rule.
	const float stripePxWidth = vp.x() / static_cast<float>(numRays);
	const float maxDistCells = std::max(1.f, g_state->config.maxDistance);
	// DDA needs an integer cell budget large enough to reach maxDistance even on near-axis rays.
	const int maxSteps = static_cast<int>(std::ceil(maxDistCells * 2.f));
	// Horizon shared with the backdrop (see `computeHorizonY`). With an integer
	// horizon and an integer line height, every stripe's pixel coverage is
	// deterministic and stable across frames.
	const float horizonY = computeHorizonY();
	const auto& tileset = *iTilemap.tileset;
	const auto& atlasTex = tileset.texture;
	// Per-column hit recorded by the DDA walk; one ray can collect multiple of these
	// when it crosses transparent tiles before reaching an opaque one. The vector is
	// declared outside the column loop so the allocation is reused across columns.
	struct ColumnHit {
		int32_t tileIndex;///< Tile index hit (>= 0).
		float perpDist;///< Perpendicular distance in cell units (already clamped to g_MinPerpDist).
		float wallX;///< Texture U fraction along the wall face.
		float wallHeight;///< Vertical scale of the wall (from `TileMeta::wallHeight`).
		int side;///< 0 = X-edge hit, 1 = Y-edge hit (side darkening cue).
		bool transparent;///< When true the DDA walked past this hit to keep collecting.
	};
	std::vector<ColumnHit> columnHits;
	// Cap the number of transparent layers a single ray can stack so a corridor of
	// see-through tiles can't blow up the per-frame stripe budget. 8 is plenty for
	// realistic raycast scenes (fences, glass, grilles overlapping) while still
	// bounding the worst case.
	constexpr size_t kMaxTransparentHits = 8;
	for (uint32_t col = 0; col < numRays; ++col) {
		const float cameraX = 2.f * (static_cast<float>(col) + 0.5f) / static_cast<float>(numRays) - 1.f;
		const math::vec2 rayDir{camCellDir.x() + camCellPlane.x() * cameraX,
								camCellDir.y() + camCellPlane.y() * cameraX};
		// Length of ray needed to cross one full cell on each axis.
		const float deltaX = (std::abs(rayDir.x()) < 1e-6f) ? 1e30f : std::abs(1.f / rayDir.x());
		const float deltaY = (std::abs(rayDir.y()) < 1e-6f) ? 1e30f : std::abs(1.f / rayDir.y());
		int mapX = static_cast<int>(std::floor(camCellPos.x()));
		int mapY = static_cast<int>(std::floor(camCellPos.y()));
		const int stepX = (rayDir.x() < 0.f) ? -1 : 1;
		const int stepY = (rayDir.y() < 0.f) ? -1 : 1;
		float sideDistX = (rayDir.x() < 0.f) ? (camCellPos.x() - static_cast<float>(mapX)) * deltaX
											 : (static_cast<float>(mapX + 1) - camCellPos.x()) * deltaX;
		float sideDistY = (rayDir.y() < 0.f) ? (camCellPos.y() - static_cast<float>(mapY)) * deltaY
											 : (static_cast<float>(mapY + 1) - camCellPos.y()) * deltaY;
		columnHits.clear();
		int side = 0;// 0: X-side, 1: Y-side
		for (int step = 0; step < maxSteps; ++step) {
			if (sideDistX < sideDistY) {
				sideDistX += deltaX;
				mapX += stepX;
				side = 0;
			} else {
				sideDistY += deltaY;
				mapY += stepY;
				side = 1;
			}
			// Bounds-checked cell read. Negative `mapX/mapY` cast to a huge `uint32_t`
			// which fails the size check in `Tilemap::getTile`, so the cast is safe and
			// resolves to `g_EmptyTileIndex` (-1) for any out-of-grid coordinate.
			const int32_t cell = iTilemap.getTile(static_cast<uint32_t>(layerIdx), static_cast<uint32_t>(mapX),
												  static_cast<uint32_t>(mapY));
			if (cell < 0)
				continue;
			const auto& meta = tileset.getTileMeta(static_cast<uint32_t>(cell));
			const float perpDistCells = (side == 0) ? (sideDistX - deltaX) : (sideDistY - deltaY);
			const float perpDistSafe = std::max(perpDistCells, g_MinPerpDist);
			float wallX = (side == 0) ? (camCellPos.y() + perpDistSafe * rayDir.y())
									  : (camCellPos.x() + perpDistSafe * rayDir.x());
			wallX -= std::floor(wallX);
			// Match Wolfenstein convention: walls hit from the +x or -y direction get U flipped
			// so that adjacent wall faces appear continuous.
			if (side == 0 && rayDir.x() > 0.f)
				wallX = 1.f - wallX;
			if (side == 1 && rayDir.y() < 0.f)
				wallX = 1.f - wallX;
			columnHits.push_back({.tileIndex = cell,
								  .perpDist = perpDistSafe,
								  .wallX = wallX,
								  .wallHeight = std::max(0.f, meta.wallHeight),
								  .side = side,
								  .transparent = meta.transparent});
			// Opaque hit terminates the ray. Transparent hits keep the ray going up to
			// the per-ray budget so the back-to-front render shows farther walls
			// through the see-through pixels of nearer ones.
			if (!meta.transparent)
				break;
			if (columnHits.size() >= kMaxTransparentHits)
				break;
		}
		g_state->stats.stripeCount++;
		if (columnHits.empty()) {
			g_state->stats.missCount++;
			continue;
		}

		// Latch the **closest opaque** wall depth for sprite occlusion. Sprites passing
		// through a column that only has transparent hits are not occluded by the
		// transparent wall (they will be drawn on top of it though — a v0.2.0 limitation
		// caused by sprites being drawn after walls instead of merged per-column).
		float opaqueDepth = std::numeric_limits<float>::infinity();
		for (const auto& hitEntry: columnHits) {
			if (!hitEntry.transparent) {
				opaqueDepth = hitEntry.perpDist;
				break;
			}
		}
		if (col < g_state->zBufferPerColumn.size())
			g_state->zBufferPerColumn[col] = opaqueDepth;

		// Render back-to-front so painter's order composites alpha-blended transparent
		// walls correctly. `columnHits` is in increasing-distance order (DDA marches
		// outward), so iterate in reverse.
		const float stripeX = (static_cast<float>(col) + 0.5f) * stripePxWidth;
		for (const auto& wallHit: std::ranges::reverse_view(columnHits)) {
			g_state->stats.hitCount++;
			// Snap *unit* line height to an even integer (existing flicker-free trick),
			// then scale by the wall's `wallHeight`. Wall is bottom-anchored at floor
			// level, so a tall wall pokes into the sky and a short one stays planted on
			// the floor — `screenCenterY` shifts upward by half the *extra* height.
			const float lineHeightUnit = std::floor((vp.y() / wallHit.perpDist) * 0.5f) * 2.f;
			const float lineHeight = lineHeightUnit * wallHit.wallHeight;
			const float screenCenterY = horizonY + lineHeightUnit * (wallHit.wallHeight - 1.f) * 0.5f;
			const auto tileUv = tileset.getTileUv(static_cast<uint32_t>(wallHit.tileIndex));
			// `getTileUv` returns BL, BR, TR, TL.
			const float uvLeft = tileUv[0].x();
			const float uvRight = tileUv[1].x();
			const float uvBottom = tileUv[0].y();
			const float uvTop = tileUv[3].y();
			const float uHit = uvLeft + wallHit.wallX * (uvRight - uvLeft);
			math::Transform stripeTr;
			stripeTr.translation() = math::vec3{stripeX, screenCenterY, 0.f};
			stripeTr.scale() = math::vec3{stripePxWidth, lineHeight, 1.f};
			math::vec4 tint{1.f, 1.f, 1.f, 1.f};
			if (wallHit.side == 1)
				tint = math::vec4{g_YSideDarken, g_YSideDarken, g_YSideDarken, 1.f};
			// One U value across the stripe (a single atlas column), tile spans full V.
			const std::array<math::vec2, 4> stripeUv{
					math::vec2{uHit, uvBottom},
					math::vec2{uHit, uvBottom},
					math::vec2{uHit, uvTop},
					math::vec2{uHit, uvTop},
			};
			Renderer2D::drawQuad({.transform = stripeTr,
								  .color = tint,
								  .texture = atlasTex,
								  .textureCoords = stripeUv,
								  .entityId = iEntityId});
		}
	}
}

namespace {

/// Slab-method intersection result for a single ray against an AABB.
struct SlabHit {
	float perpDist;///< Entry distance along the camera ray (perpendicular-to-plane).
	int side;///< 0 = X slab entered first, 1 = Y slab entered first.
	float wallU;///< Texture U fraction along the hit face (already flipped for Wolf3D convention).
	bool valid;///< False if the ray missed the AABB or the hit is behind the camera.
};

/// Run the slab method on one AABB for one ray. Used by both `drawDynamicWalls` and `drawDoors`.
auto castRayAabb(const math::vec2& iRayDir, float iMinX, float iMaxX, float iMinY, float iMaxY) -> SlabHit {
	// Slab method against the AABB. Both X and Y slabs guard against degenerate rays
	// (rayDir.x or rayDir.y ≈ 0) with a 1/eps clamp: when the ray is parallel to a
	// slab and the origin sits outside the slab, both slab intervals collapse to ±inf
	// in the right order and the overall `tNear > tFar` test still rejects.
	const float invDx = (std::abs(iRayDir.x()) < 1e-6f) ? std::copysign(1e30f, iRayDir.x() == 0.f ? 1.f : iRayDir.x())
														: 1.f / iRayDir.x();
	const float invDy = (std::abs(iRayDir.y()) < 1e-6f) ? std::copysign(1e30f, iRayDir.y() == 0.f ? 1.f : iRayDir.y())
														: 1.f / iRayDir.y();
	const float tx1 = (iMinX - g_state->cameraPos2D.x()) * invDx;
	const float tx2 = (iMaxX - g_state->cameraPos2D.x()) * invDx;
	const float tEnterX = std::min(tx1, tx2);
	const float tExitX = std::max(tx1, tx2);
	const float ty1 = (iMinY - g_state->cameraPos2D.y()) * invDy;
	const float ty2 = (iMaxY - g_state->cameraPos2D.y()) * invDy;
	const float tEnterY = std::min(ty1, ty2);
	const float tExitY = std::max(ty1, ty2);
	const float tNear = std::max(tEnterX, tEnterY);
	const float tFar = std::min(tExitX, tExitY);
	if (tNear > tFar || tFar < 0.f)
		return {.perpDist = 0.f, .side = 0, .wallU = 0.f, .valid = false};
	const float perpDist = std::max(tNear, g_MinPerpDist);
	const int side = (tEnterX > tEnterY) ? 0 : 1;
	float wallU = 0.f;
	if (side == 0) {
		const float hitY = g_state->cameraPos2D.y() + perpDist * iRayDir.y();
		wallU = (hitY - iMinY) / (iMaxY - iMinY);
		if (iRayDir.x() > 0.f)
			wallU = 1.f - wallU;
	} else {
		const float hitX = g_state->cameraPos2D.x() + perpDist * iRayDir.x();
		wallU = (hitX - iMinX) / (iMaxX - iMinX);
		if (iRayDir.y() < 0.f)
			wallU = 1.f - wallU;
	}
	wallU = std::clamp(wallU, 0.f, 1.f);
	return {.perpDist = perpDist, .side = side, .wallU = wallU, .valid = true};
}

/**
 * @brief
 *  Emit a single wall stripe for one column and refresh the zBuffer.
 *
 * `iUvRect` is the (minU, minV, maxU, maxV) sub-rectangle of `iTexture` the
 * stripe samples from — that's how a single atlas texture can serve many
 * tiles. The stripe's local `[0,1]` U,V are remapped into this rectangle
 * before half-texel inset (which is done in atlas pixel space, so the inset
 * value is the same whether or not we're sampling a sub-rect).
 */
void emitStripe(uint32_t iCol, float iStripeX, float iStripePxWidth, float iHorizonY, const math::vec2& iVp,
				float iPerpDist, int iSide, float iWallU, float iWallHeight, const shared<gpu::Texture>& iTexture,
				const math::vec4& iUvRect, const math::vec4& iTint, int iEntityId) {
	const float lineHeightUnit = std::floor((iVp.y() / iPerpDist) * 0.5f) * 2.f;
	const float lineHeight = lineHeightUnit * iWallHeight;
	const float screenCenterY = iHorizonY + lineHeightUnit * (iWallHeight - 1.f) * 0.5f;
	const auto texSize = iTexture->getSize();
	const float halfU = texSize.x() > 0 ? 0.5f / static_cast<float>(texSize.x()) : 0.f;
	const float halfV = texSize.y() > 0 ? 0.5f / static_cast<float>(texSize.y()) : 0.f;
	// Remap local [0,1] U,V into the sub-rect, then inset half a texel on each
	// side so bilinear sampling doesn't bleed in the neighbouring tile of the atlas.
	const float subUMin = iUvRect.x() + halfU;
	const float subUMax = iUvRect.z() - halfU;
	const float subVMin = iUvRect.y() + halfV;
	const float subVMax = iUvRect.w() - halfV;
	const float clampedLocalU = std::clamp(iWallU, 0.f, 1.f);
	const float uHit = subUMin + clampedLocalU * (subUMax - subUMin);
	const float vBottom = subVMin;
	const float vTop = subVMax;
	math::Transform stripeTr;
	stripeTr.translation() = math::vec3{iStripeX, screenCenterY, 0.f};
	stripeTr.scale() = math::vec3{iStripePxWidth, lineHeight, 1.f};
	math::vec4 tint = iTint;
	if (iSide == 1) {
		tint.x() *= g_YSideDarken;
		tint.y() *= g_YSideDarken;
		tint.z() *= g_YSideDarken;
	}
	const std::array<math::vec2, 4> stripeUv{
			math::vec2{uHit, vBottom},
			math::vec2{uHit, vBottom},
			math::vec2{uHit, vTop},
			math::vec2{uHit, vTop},
	};
	Renderer2D::drawQuad({.transform = stripeTr,
						  .color = tint,
						  .texture = iTexture,
						  .textureCoords = stripeUv,
						  .entityId = iEntityId});
	if (iCol < g_state->zBufferPerColumn.size())
		g_state->zBufferPerColumn[iCol] = iPerpDist;
}

}// namespace

void RendererRaycast::drawDynamicWalls(std::span<const RaycastDynamicWallData> iWalls) {
	OWL_PROFILE_FUNCTION()

	if (!g_state || !g_state->sceneOpen) {
		OWL_CORE_WARN("RendererRaycast::drawDynamicWalls called outside beginScene/endScene.")
		return;
	}
	if (iWalls.empty())
		return;
	// A dynamic wall draws against the same backdrop the static-wall pass uses; if
	// the caller skipped tilemap drawing (raycast scene with only dynamic walls)
	// emit the sky / floor here so the wall doesn't sit on stale framebuffer data.
	emitBackdropIfNeeded();

	const math::vec2 vp{static_cast<float>(g_state->viewport.x()), static_cast<float>(g_state->viewport.y())};
	const uint32_t numRays = (g_state->config.numRays > 0) ? g_state->config.numRays : g_state->viewport.x();
	if (numRays == 0)
		return;
	const float stripePxWidth = vp.x() / static_cast<float>(numRays);
	const float horizonY = computeHorizonY();
	const float maxDistance = std::max(1.f, g_state->config.maxDistance);
	const math::vec2& dir = g_state->cameraDir2D;
	const math::vec2& plane = g_state->cameraPlane2D;

	for (const auto& wall: iWalls) {
		if (!wall.texture)
			continue;
		const float halfX = std::max(1e-4f, wall.halfExtent.x());
		const float halfY = std::max(1e-4f, wall.halfExtent.y());
		const float minX = wall.worldCenter.x() - halfX;
		const float maxX = wall.worldCenter.x() + halfX;
		const float minY = wall.worldCenter.y() - halfY;
		const float maxY = wall.worldCenter.y() + halfY;
		const float wallHeightScale = std::max(0.f, wall.wallHeight);
		bool wallEmittedAny = false;
		for (uint32_t col = 0; col < numRays; ++col) {
			const float cameraX = 2.f * (static_cast<float>(col) + 0.5f) / static_cast<float>(numRays) - 1.f;
			const math::vec2 rayDir{dir.x() + plane.x() * cameraX, dir.y() + plane.y() * cameraX};
			const auto hit = castRayAabb(rayDir, minX, maxX, minY, maxY);
			if (!hit.valid)
				continue;
			if (hit.perpDist > maxDistance)
				continue;
			const float currentZ = (col < g_state->zBufferPerColumn.size()) ? g_state->zBufferPerColumn[col]
																			: std::numeric_limits<float>::infinity();
			if (hit.perpDist >= currentZ)
				continue;
			const float stripeX = (static_cast<float>(col) + 0.5f) * stripePxWidth;
			emitStripe(col, stripeX, stripePxWidth, horizonY, vp, hit.perpDist, hit.side, hit.wallU, wallHeightScale,
					   wall.texture, wall.uvRect, wall.tint, wall.entityId);
			g_state->stats.dynamicWallStripeCount++;
			wallEmittedAny = true;
		}
		if (wallEmittedAny)
			g_state->stats.dynamicWallCount++;
	}
}

namespace {

/**
 * @brief
 *  Geometry of a single door for the renderer — derived once per door per call.
 *
 * Holds the cube extents, the plate offset along the slide axis (with the
 * scaled-by-progress +1-pixel hermetic-closure margin already applied), and
 * the slide-axis classification used to dispatch the plate / lateral tests.
 */
struct DoorGeom {
	float cellMinX;///< Cube left edge in world cells.
	float cellMaxX;///< Cube right edge.
	float cellMinY;///< Cube bottom edge.
	float cellMaxY;///< Cube top edge.
	float plateAlongSlide;///< Plate position along the slide axis (signed, cell units).
	float slideSign;///< +1 (N or E opening) / −1 (S or W opening).
	bool slideAlongY;///< True for N/S openings (plate normal along X), false for E/W.
};

/// Cube-AABB intersection result reused inside `drawDoors` — holds entry / exit / exit-axis.
struct CubeHit {
	float tNear;///< Entry distance (cells, perp-to-plane).
	float tFar;///< Exit distance.
	float invDx;///< Cached `1 / rayDir.x` (clamped on degenerate rays).
	float invDy;///< Cached `1 / rayDir.y`.
	int exitAxis;///< 0 if X-slab closes first, 1 if Y-slab closes first.
	bool valid;///< False when the ray misses the cube or the exit is behind the camera.
};

auto castCubeAabb(const math::vec2& iRayDir, const DoorGeom& iGeom) -> CubeHit {
	const float invDx = (std::abs(iRayDir.x()) < 1e-6f) ? std::copysign(1e30f, iRayDir.x() == 0.f ? 1.f : iRayDir.x())
														: 1.f / iRayDir.x();
	const float invDy = (std::abs(iRayDir.y()) < 1e-6f) ? std::copysign(1e30f, iRayDir.y() == 0.f ? 1.f : iRayDir.y())
														: 1.f / iRayDir.y();
	const float tx1 = (iGeom.cellMinX - g_state->cameraPos2D.x()) * invDx;
	const float tx2 = (iGeom.cellMaxX - g_state->cameraPos2D.x()) * invDx;
	const float tExitX = std::max(tx1, tx2);
	const float ty1 = (iGeom.cellMinY - g_state->cameraPos2D.y()) * invDy;
	const float ty2 = (iGeom.cellMaxY - g_state->cameraPos2D.y()) * invDy;
	const float tExitY = std::max(ty1, ty2);
	const float tNear = std::max(std::min(tx1, tx2), std::min(ty1, ty2));
	const float tFar = std::min(tExitX, tExitY);
	const bool valid = (tNear <= tFar) && (tFar >= 0.f);
	const int exitAxis = (tExitX < tExitY) ? 0 : 1;
	return {.tNear = tNear, .tFar = tFar, .invDx = invDx, .invDy = invDy, .exitAxis = exitAxis, .valid = valid};
}

/// Plate intersection result emitted by `tryPlateHit`.
struct PlateHit {
	float t;///< Distance along the ray (cells, perp-to-plane).
	float pixelU;///< 0 at min-axis edge of the plate, 1 at max-axis edge.
	bool valid;
};

auto tryPlateHit(const math::vec2& iRayDir, const DoorGeom& iGeom, const CubeHit& iCube, const RaycastDoorData& iDoor)
		-> PlateHit {
	if (!iDoor.faceTexture)
		return {.t = 0.f, .pixelU = 0.f, .valid = false};
	if (iGeom.slideAlongY) {
		// N/S opening: plate at x = cx, extends along Y.
		if (std::abs(iRayDir.x()) <= 1e-6f)
			return {.t = 0.f, .pixelU = 0.f, .valid = false};
		const float t = (iDoor.cellCenter.x() - g_state->cameraPos2D.x()) * iCube.invDx;
		if (t < iCube.tNear || t > iCube.tFar || t <= 0.f)
			return {.t = 0.f, .pixelU = 0.f, .valid = false};
		const float hitY = g_state->cameraPos2D.y() + t * iRayDir.y();
		const float plateCenterY = iDoor.cellCenter.y() + iGeom.plateAlongSlide;
		const float plateMinY = plateCenterY - 0.5f;
		const float plateMaxY = plateCenterY + 0.5f;
		if (hitY < plateMinY || hitY > plateMaxY)
			return {.t = 0.f, .pixelU = 0.f, .valid = false};
		return {.t = std::max(t, g_MinPerpDist), .pixelU = std::clamp(hitY - plateMinY, 0.f, 1.f), .valid = true};
	}
	// E/W opening: plate at y = cy, extends along X.
	if (std::abs(iRayDir.y()) <= 1e-6f)
		return {.t = 0.f, .pixelU = 0.f, .valid = false};
	const float t = (iDoor.cellCenter.y() - g_state->cameraPos2D.y()) * iCube.invDy;
	if (t < iCube.tNear || t > iCube.tFar || t <= 0.f)
		return {.t = 0.f, .pixelU = 0.f, .valid = false};
	const float hitX = g_state->cameraPos2D.x() + t * iRayDir.x();
	const float plateCenterX = iDoor.cellCenter.x() + iGeom.plateAlongSlide;
	const float plateMinX = plateCenterX - 0.5f;
	const float plateMaxX = plateCenterX + 0.5f;
	if (hitX < plateMinX || hitX > plateMaxX)
		return {.t = 0.f, .pixelU = 0.f, .valid = false};
	return {.t = std::max(t, g_MinPerpDist), .pixelU = std::clamp(hitX - plateMinX, 0.f, 1.f), .valid = true};
}

/// Lateral intersection result emitted by `tryLateralHit`.
struct LateralHit {
	float t;
	float wallU;
	int side;
	bool valid;
};

auto tryLateralHit(const math::vec2& iRayDir, const DoorGeom& iGeom, const CubeHit& iCube, const RaycastDoorData& iDoor)
		-> LateralHit {
	if (!iDoor.lateralTexture || iCube.tFar <= 0.f)
		return {.t = 0.f, .wallU = 0.f, .side = 0, .valid = false};
	const bool exitOnSlideAxis = iGeom.slideAlongY ? (iCube.exitAxis == 1) : (iCube.exitAxis == 0);
	if (!exitOnSlideAxis)
		return {.t = 0.f, .wallU = 0.f, .side = 0, .valid = false};
	// Tiny depth bias so the lateral always wins the z-test against the pocket-side
	// wall (which sits at exactly the same depth in the static tilemap pass).
	constexpr float kLateralDepthBias = 1e-3f;
	const float t = std::max(g_MinPerpDist, iCube.tFar - kLateralDepthBias);
	float wallU = 0.f;
	int side = 0;
	if (iGeom.slideAlongY) {
		const float hitX = g_state->cameraPos2D.x() + iCube.tFar * iRayDir.x();
		wallU = hitX - iGeom.cellMinX;
		side = 1;// match standard Y-slab darkening
		if (iRayDir.y() < 0.f)
			wallU = 1.f - wallU;
	} else {
		const float hitY = g_state->cameraPos2D.y() + iCube.tFar * iRayDir.y();
		wallU = hitY - iGeom.cellMinY;
		side = 0;
		if (iRayDir.x() > 0.f)
			wallU = 1.f - wallU;
	}
	return {.t = t, .wallU = std::clamp(wallU, 0.f, 1.f), .side = side, .valid = true};
}

/// Common per-column input bundle for the door render loop.
struct DoorColumnCtx {
	math::vec2 rayDir;
	float currentZ;
	float maxDistance;
};

/**
 * @brief
 *  Pick the closest of {plate, lateral} that beats the static zBuffer and emit a stripe.
 *
 * Returns true when a stripe was actually emitted (used to bump the per-door
 * stripe-emitted flag). Doing the picking + emit in this helper keeps
 * `drawDoors`'s top-level loop a flat sequence of `if (cube.valid) { … }`,
 * well under the cognitive-complexity threshold.
 */
auto renderDoorColumn(uint32_t iCol, float iStripeX, float iStripePxWidth, float iHorizonY, const math::vec2& iVp,
					  float iWallHeightScale, const DoorGeom& iGeom, const PlateHit& iPlate, const LateralHit& iLateral,
					  const DoorColumnCtx& iCtx, const RaycastDoorData& iDoor) -> bool {
	float bestT = std::numeric_limits<float>::infinity();
	float bestU = 0.f;
	int bestSide = 0;
	const shared<gpu::Texture>* bestTex = nullptr;
	const math::vec4* bestUvRect = nullptr;
	if (iPlate.valid && iPlate.t < iCtx.currentZ && iPlate.t <= iCtx.maxDistance) {
		bestT = iPlate.t;
		// U=1 must always sit on the opening-direction side of the cell. `pixelU` runs
		// 0 (south/west) → 1 (north/east); for negative-direction openings (S/W) we
		// flip so U=1 sits on the opening side regardless of the player's viewpoint.
		float u = iPlate.pixelU;
		if (iGeom.slideSign < 0.f)
			u = 1.f - u;
		bestU = std::clamp(u, 0.f, 1.f);
		bestSide = iGeom.slideAlongY ? 0 : 1;
		bestTex = &iDoor.faceTexture;
		bestUvRect = &iDoor.faceUvRect;
	}
	if (iLateral.valid && iLateral.t < bestT && iLateral.t < iCtx.currentZ && iLateral.t <= iCtx.maxDistance) {
		bestT = iLateral.t;
		bestU = iLateral.wallU;
		bestSide = iLateral.side;
		bestTex = &iDoor.lateralTexture;
		bestUvRect = &iDoor.lateralUvRect;
	}
	if (bestTex == nullptr || !*bestTex || bestUvRect == nullptr)
		return false;
	emitStripe(iCol, iStripeX, iStripePxWidth, iHorizonY, iVp, bestT, bestSide, bestU, iWallHeightScale, *bestTex,
			   *bestUvRect, iDoor.tint, iDoor.entityId);
	return true;
}

auto buildDoorGeom(const RaycastDoorData& iDoor) -> DoorGeom {
	// Decode opening direction into a slide axis + sign.
	// 0 = North (+Y), 1 = South (−Y), 2 = East (+X), 3 = West (−X).
	constexpr float kPlatePixelMargin = 1.f / 64.f;
	const bool slideAlongY = iDoor.openingDirection <= 1;
	const float slideSign = (iDoor.openingDirection == 0 || iDoor.openingDirection == 2) ? 1.f : -1.f;
	// Scale the +1-pixel margin by the open progress so a closed door (offset=0)
	// stays exactly at the cell centre and closes hermetically.
	const float plateAlongSlide = slideSign * iDoor.plateOffset * (1.f + kPlatePixelMargin);
	return {.cellMinX = iDoor.cellCenter.x() - 0.5f,
			.cellMaxX = iDoor.cellCenter.x() + 0.5f,
			.cellMinY = iDoor.cellCenter.y() - 0.5f,
			.cellMaxY = iDoor.cellCenter.y() + 0.5f,
			.plateAlongSlide = plateAlongSlide,
			.slideSign = slideSign,
			.slideAlongY = slideAlongY};
}

}// namespace

void RendererRaycast::drawDoors(std::span<const RaycastDoorData> iDoors) {
	OWL_PROFILE_FUNCTION()

	if (!g_state || !g_state->sceneOpen) {
		OWL_CORE_WARN("RendererRaycast::drawDoors called outside beginScene/endScene.")
		return;
	}
	if (iDoors.empty())
		return;
	emitBackdropIfNeeded();

	const math::vec2 vp{static_cast<float>(g_state->viewport.x()), static_cast<float>(g_state->viewport.y())};
	const uint32_t numRays = (g_state->config.numRays > 0) ? g_state->config.numRays : g_state->viewport.x();
	if (numRays == 0)
		return;
	const float stripePxWidth = vp.x() / static_cast<float>(numRays);
	const float horizonY = computeHorizonY();
	const float maxDistance = std::max(1.f, g_state->config.maxDistance);
	const math::vec2& dir = g_state->cameraDir2D;
	const math::vec2& plane = g_state->cameraPlane2D;

	for (const auto& door: iDoors) {
		if (!door.faceTexture && !door.lateralTexture)
			continue;
		const DoorGeom geom = buildDoorGeom(door);
		const float wallHeightScale = std::max(0.f, door.wallHeight);
		bool doorEmittedAny = false;
		for (uint32_t col = 0; col < numRays; ++col) {
			const float cameraX = 2.f * (static_cast<float>(col) + 0.5f) / static_cast<float>(numRays) - 1.f;
			const math::vec2 rayDir{dir.x() + plane.x() * cameraX, dir.y() + plane.y() * cameraX};
			const CubeHit cube = castCubeAabb(rayDir, geom);
			if (!cube.valid)
				continue;
			const PlateHit plate = tryPlateHit(rayDir, geom, cube, door);
			const LateralHit lateral = tryLateralHit(rayDir, geom, cube, door);
			const float currentZ = (col < g_state->zBufferPerColumn.size()) ? g_state->zBufferPerColumn[col]
																			: std::numeric_limits<float>::infinity();
			const DoorColumnCtx ctx{.rayDir = rayDir, .currentZ = currentZ, .maxDistance = maxDistance};
			const float stripeX = (static_cast<float>(col) + 0.5f) * stripePxWidth;
			if (renderDoorColumn(col, stripeX, stripePxWidth, horizonY, vp, wallHeightScale, geom, plate, lateral, ctx,
								 door)) {
				g_state->stats.doorStripeCount++;
				doorEmittedAny = true;
			}
		}
		if (doorEmittedAny)
			g_state->stats.doorCount++;
	}
}

void RendererRaycast::drawSprites(std::span<const RaycastSpriteData> iSprites) {
	OWL_PROFILE_FUNCTION()

	if (!g_state || !g_state->sceneOpen) {
		OWL_CORE_WARN("RendererRaycast::drawSprites called outside beginScene/endScene.")
		return;
	}
	if (iSprites.empty())
		return;
	// Sprites alone don't trigger the wall path, but we still want a backdrop behind
	// them when the scene has no tilemap (otherwise sprites float on whatever the
	// previous layer left in the framebuffer).
	emitBackdropIfNeeded();

	// Camera-space basis: invert `[plane | dir]` so we can express any world point
	// as `(transformX, transformY)` in camera coordinates — `transformY` is the
	// forward distance (used for depth-sort + occlusion + perspective scale),
	// `transformX` is the lateral offset that drives the screen-X projection.
	const math::vec2& dir = g_state->cameraDir2D;
	const math::vec2& plane = g_state->cameraPlane2D;
	const float det = plane.x() * dir.y() - dir.x() * plane.y();
	if (std::abs(det) < 1e-8f)
		return;
	const float invDet = 1.f / det;

	struct Projected {
		size_t spriteIdx;///< Index into the input span.
		float transformX;///< Lateral camera-space offset.
		float transformY;///< Forward camera-space distance (always > 0 after culling).
	};
	std::vector<Projected> visible;
	visible.reserve(iSprites.size());

	const float maxDistance = std::max(1.f, g_state->config.maxDistance);
	for (size_t i = 0; i < iSprites.size(); ++i) {
		const auto& sprite = iSprites[i];
		if (!sprite.texture)
			continue;
		const float dx = sprite.worldPosition.x() - g_state->cameraPos2D.x();
		const float dy = sprite.worldPosition.y() - g_state->cameraPos2D.y();
		const float transformX = invDet * (dir.y() * dx - dir.x() * dy);
		const float transformY = invDet * (-plane.y() * dx + plane.x() * dy);
		if (transformY <= g_MinPerpDist)
			continue;
		if (transformY > maxDistance)
			continue;
		visible.push_back({i, transformX, transformY});
	}
	if (visible.empty())
		return;

	// Painter's order: farther sprites first so closer sprites overdraw them.
	std::ranges::sort(visible, [](const Projected& iLhs, const Projected& iRhs) -> bool {
		return iLhs.transformY > iRhs.transformY;
	});

	const math::vec2 vp{static_cast<float>(g_state->viewport.x()), static_cast<float>(g_state->viewport.y())};
	const uint32_t numRays = (g_state->config.numRays > 0) ? g_state->config.numRays : g_state->viewport.x();
	if (numRays == 0)
		return;
	const float stripePxWidth = vp.x() / static_cast<float>(numRays);
	const float horizonY = computeHorizonY();
	const auto& zBuffer = g_state->zBufferPerColumn;

	for (const auto& projected: visible) {
		const auto& sprite = iSprites[projected.spriteIdx];
		const float invTy = 1.f / projected.transformY;
		const float spriteH = std::max(0.f, sprite.worldSize.y()) * vp.y() * invTy;
		const float spriteW = std::max(0.f, sprite.worldSize.x()) * vp.y() * invTy;
		if (spriteH < 1.f || spriteW < 1.f)
			continue;
		const float screenCenterX = vp.x() * 0.5f * (1.f + projected.transformX * invTy);
		const float screenCenterY = horizonY + sprite.worldZOffset * vp.y() * invTy;
		const float screenLeft = screenCenterX - spriteW * 0.5f;
		const float screenRight = screenCenterX + spriteW * 0.5f;
		const int colStart = std::max(0, static_cast<int>(std::floor(screenLeft / stripePxWidth)));
		const int colEnd =
				std::min(static_cast<int>(numRays), static_cast<int>(std::ceil(screenRight / stripePxWidth)));
		if (colStart >= colEnd)
			continue;
		const auto& tc = sprite.textureCoords;
		// Inset the sprite-cell UV rectangle by half a texel: the engine samples
		// every texture with `REPEAT` wrap mode + bilinear filtering, so a UV at
		// the very edge of a sprite bleeds in the opposite-edge column (single
		// sprites) or the neighbouring cell of an atlas (animated sprites).
		const auto texSize = sprite.texture->getSize();
		const float halfU = texSize.x() > 0 ? 0.5f / static_cast<float>(texSize.x()) : 0.f;
		const float halfV = texSize.y() > 0 ? 0.5f / static_cast<float>(texSize.y()) : 0.f;
		const float uvLeft = tc[0].x() + halfU;
		const float uvRight = tc[1].x() - halfU;
		const float uvBottom = tc[0].y() + halfV;
		const float uvTop = tc[3].y() - halfV;
		bool spriteEmittedAny = false;
		for (int col = colStart; col < colEnd; ++col) {
			const float wallDepth = (static_cast<size_t>(col) < zBuffer.size())
											? zBuffer[static_cast<size_t>(col)]
											: std::numeric_limits<float>::infinity();
			if (projected.transformY >= wallDepth) {
				g_state->stats.spriteOccludedCount++;
				continue;
			}
			const float colCenterX = (static_cast<float>(col) + 0.5f) * stripePxWidth;
			const float localX = colCenterX - screenLeft;
			const float u = std::clamp(localX / spriteW, 0.f, 1.f);
			const float uHit = uvLeft + u * (uvRight - uvLeft);

			math::Transform stripeTr;
			stripeTr.translation() = math::vec3{colCenterX, screenCenterY, 0.f};
			stripeTr.scale() = math::vec3{stripePxWidth, spriteH, 1.f};
			const std::array<math::vec2, 4> stripeUv{
					math::vec2{uHit, uvBottom},
					math::vec2{uHit, uvBottom},
					math::vec2{uHit, uvTop},
					math::vec2{uHit, uvTop},
			};
			Renderer2D::drawQuad({.transform = stripeTr,
								  .color = sprite.tint,
								  .texture = sprite.texture,
								  .textureCoords = stripeUv,
								  .entityId = sprite.entityId});
			g_state->stats.spriteStripeCount++;
			spriteEmittedAny = true;
		}
		if (spriteEmittedAny)
			g_state->stats.spriteCount++;
	}
}

void RendererRaycast::endScene() {
	if (!g_state)
		return;
	g_state->sceneOpen = false;
	// Renderer2D batch is flushed by the layer's onEndFrame — nothing to do here.
}

void RendererRaycast::resetStats() {
	if (!g_state)
		return;
	g_state->stats = Statistics{};
}

auto RendererRaycast::getStats() -> Statistics {
	if (!g_state)
		return Statistics{};
	return g_state->stats;
}

}// namespace owl::renderer
