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
#include "scene/Tileset.h"
#include "scene/component/Tilemap.h"

#include <cmath>

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
auto pickActiveLayerIndex(const scene::component::Tilemap& iTilemap) -> size_t {
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

void RendererRaycast::drawTilemapWalls(const scene::component::Tilemap& iTilemap,
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
		bool hit = false;
		int side = 0;// 0: X-side, 1: Y-side
		int32_t hitTile = scene::component::g_EmptyTileIndex;
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
			if (cell >= 0) {
				hit = true;
				hitTile = cell;
				break;
			}
		}
		g_state->stats.stripeCount++;
		if (!hit) {
			g_state->stats.missCount++;
			continue;
		}
		g_state->stats.hitCount++;
		// Perpendicular distance (avoid fish-eye): subtract last delta on the axis we just stepped.
		const float perpDistCells = (side == 0) ? (sideDistX - deltaX) : (sideDistY - deltaY);
		const float perpDistSafe = std::max(perpDistCells, g_MinPerpDist);
		// Snap line height to an even integer. Combined with the integer horizon
		// it guarantees the stripe top / bottom land on integer pixel boundaries
		// every frame, no matter how the camera's float-precision distance shifts —
		// without snapping, sub-pixel oscillations of `vpH / perpDist` make the
		// stripe edges flicker between two neighbour rows as the player walks.
		const float lineHeightExact = vp.y() / perpDistSafe;
		const float lineHeight = std::floor(lineHeightExact * 0.5f) * 2.f;
		const float screenCenterY = horizonY;
		// gpu::Texture U coordinate within the wall: where on the wall did we hit?
		float wallX = (side == 0) ? (camCellPos.y() + perpDistSafe * rayDir.y())
								  : (camCellPos.x() + perpDistSafe * rayDir.x());
		wallX -= std::floor(wallX);
		// Match Wolfenstein convention: walls hit from the +x or -y direction get U flipped
		// so that adjacent wall faces appear continuous.
		if (side == 0 && rayDir.x() > 0.f)
			wallX = 1.f - wallX;
		if (side == 1 && rayDir.y() < 0.f)
			wallX = 1.f - wallX;
		const auto tileUv = tileset.getTileUv(static_cast<uint32_t>(hitTile));
		// `getTileUv` returns BL, BR, TR, TL.
		const float uvLeft = tileUv[0].x();
		const float uvRight = tileUv[1].x();
		const float uvBottom = tileUv[0].y();
		const float uvTop = tileUv[3].y();
		const float uHit = uvLeft + wallX * (uvRight - uvLeft);
		// Stripe quad (1 column wide, lineHeight pixels tall, centred on the horizon).
		// stripeX placed at the integer pixel-centre of the column we're filling so
		// that adjacent stripes tile against integer pixel boundaries.
		const float stripeX = (static_cast<float>(col) + 0.5f) * stripePxWidth;
		math::Transform stripeTr;
		stripeTr.translation() = math::vec3{stripeX, screenCenterY, 0.f};
		stripeTr.scale() = math::vec3{stripePxWidth, lineHeight, 1.f};
		math::vec4 tint{1.f, 1.f, 1.f, 1.f};
		if (side == 1) {
			tint = math::vec4{g_YSideDarken, g_YSideDarken, g_YSideDarken, 1.f};
		}
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
