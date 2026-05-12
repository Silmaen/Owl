/**
 * @file RendererRaycast_test.cpp
 * @author Silmaen
 * @date 04/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Application.h>
#include <renderer/CameraOrtho.h>
#include <renderer/RenderLayerFactory.h>
#include <renderer/Renderer.h>
#include <renderer/RendererRaycast.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>
#include <scene/component/Tilemap.h>

#include "renderer/RendererRaycastLayer.h"

using namespace owl;
using owl::renderer::CameraOrtho;
using owl::renderer::RaycastConfig;
using owl::renderer::RaycastDynamicWallData;
using owl::renderer::RaycastSpriteData;
using owl::renderer::Renderer;
using owl::renderer::RendererRaycast;
using owl::renderer::RendererRaycastLayer;
using owl::renderer::RenderLayerFactory;
using owl::scene::TilemapAsset;
using owl::scene::Tileset;
using owl::scene::component::TilemapLayer;

namespace {
auto makeSpriteTexture() -> shared<owl::renderer::gpu::Texture2D> {
	return owl::renderer::gpu::Texture2D::create(
			owl::renderer::gpu::Texture::Specification{.size = {16, 16},
													   .format = owl::renderer::gpu::ImageFormat::Rgba8});
}

/**
 * @brief
 *  Test-only `RaycastConfig` builder.
 *
 * Wraps value-init + per-field assignment so the call sites stay one-line
 * without designated-init lists — clang's `-Wmissing-designated-field-initializers`
 * fires on every omitted field in a partial designated init, even when the
 * struct field has a default member initializer. Tests only care about
 * `fovDegrees` / `maxDistance` / `numRays`; every other field keeps its
 * struct default.
 * @param[in] iFov Horizontal FOV in degrees.
 * @param[in] iMaxDist Max DDA distance in cells.
 * @param[in] iNumRays Number of rays (= screen columns).
 * @return The populated config.
 */
auto makeTestConfig(const float iFov, const float iMaxDist, const uint32_t iNumRays) -> RaycastConfig {
	RaycastConfig cfg;
	cfg.fovDegrees = iFov;
	cfg.maxDistance = iMaxDist;
	cfg.numRays = iNumRays;
	return cfg;
}
}// namespace

namespace {
/// Build a simple `TilemapAsset` filled with a single-row corridor of walls along
/// the +X axis at cellY = 5, 16 cells wide × 16 cells tall, cellSize = 1.
auto makeCorridorTilemap() -> TilemapAsset {
	TilemapAsset tm;
	tm.width = 16;
	tm.height = 16;
	tm.cellSize = 1.f;

	auto tileset = mkShared<Tileset>();
	tileset->columns = 4;
	tileset->rows = 4;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(tileset->tileCount());
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});
	tm.tileset = tileset;

	TilemapLayer layer;
	layer.name = "walls";
	layer.visible = true;
	layer.parallax = math::vec2{1.f, 1.f};
	layer.tiles.assign(tm.width * tm.height, owl::scene::component::g_EmptyTileIndex);
	// Fill the row at y=5 with wall tiles (tile index 1) — that's the one row that
	// will produce hits when the camera looks along +Y from below.
	for (uint32_t x = 0; x < tm.width; ++x) layer.tiles[5u * tm.width + x] = 1;
	tm.layers.push_back(std::move(layer));
	return tm;
}

/// Initialise the Null backend, the Renderer, Renderer2D, and RendererRaycast.
auto bootRendererStack() -> void {
	core::Log::init(core::Log::Level::Off);
	renderer::gpu::RenderCommand::create(renderer::gpu::RenderAPI::Type::Null);
	Renderer::init();
}

auto teardownRendererStack() -> void {
	Renderer::shutdown();
	renderer::gpu::RenderCommand::invalidate();
	core::Log::invalidate();
}

}// namespace

TEST(RendererRaycast, factoryRegistersRendererRaycastTypeKey) {
	bootRendererStack();
	EXPECT_TRUE(RenderLayerFactory::hasType("RendererRaycast"));
	const auto layer = RenderLayerFactory::create("RendererRaycast", "world");
	ASSERT_NE(layer, nullptr);
	EXPECT_STREQ(layer->getTypeKey(), "RendererRaycast");
	EXPECT_EQ(layer->getName(), "world");
	teardownRendererStack();
}

TEST(RendererRaycast, applyConfigParsesYaml) {
	bootRendererStack();
	auto layerBase = RenderLayerFactory::create("RendererRaycast", "world");
	ASSERT_NE(layerBase, nullptr);
	auto* layer = dynamic_cast<RendererRaycastLayer*>(layerBase.get());
	ASSERT_NE(layer, nullptr);

	YAML::Node cfg;
	cfg["Fov"] = 90.f;
	cfg["MaxDistance"] = 32.f;
	cfg["NumRays"] = 320;
	YAML::Node sky{YAML::NodeType::Sequence};
	sky.push_back(0.1f);
	sky.push_back(0.2f);
	sky.push_back(0.3f);
	sky.push_back(1.f);
	cfg["CeilingColor"] = sky;
	YAML::Node floor{YAML::NodeType::Sequence};
	floor.push_back(0.4f);
	floor.push_back(0.4f);
	floor.push_back(0.4f);
	floor.push_back(1.f);
	cfg["FloorColor"] = floor;

	layer->applyConfig(cfg);
	const auto& parsed = layer->getConfig();
	EXPECT_FLOAT_EQ(parsed.fovDegrees, 90.f);
	EXPECT_FLOAT_EQ(parsed.maxDistance, 32.f);
	EXPECT_EQ(parsed.numRays, 320u);
	EXPECT_FLOAT_EQ(parsed.ceilingColor.x(), 0.1f);
	EXPECT_FLOAT_EQ(parsed.floorColor.x(), 0.4f);
	teardownRendererStack();
}

TEST(RendererRaycast, setViewportLatchesNonZeroSize) {
	bootRendererStack();
	auto layerBase = RenderLayerFactory::create("RendererRaycast", "world");
	auto* layer = dynamic_cast<RendererRaycastLayer*>(layerBase.get());
	ASSERT_NE(layer, nullptr);
	layer->setViewport({800, 600});
	EXPECT_EQ(layer->getViewport().x(), 800u);
	EXPECT_EQ(layer->getViewport().y(), 600u);
	// Zero sizes are ignored to keep a sane fallback when the viewport hasn't
	// been initialised yet.
	layer->setViewport({0, 0});
	EXPECT_EQ(layer->getViewport().x(), 800u);
	EXPECT_EQ(layer->getViewport().y(), 600u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawWithEmptyTilemapDoesNothing) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	const TilemapAsset empty;
	RendererRaycast::drawTilemapWalls(empty, math::Transform{}, /*entityId=*/-1);
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.drawCalls, 0u);
	EXPECT_EQ(stats.stripeCount, 0u);
	EXPECT_EQ(stats.hitCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, missAllRaysWhenSurroundedByEmptyCells) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	// 4×4 fully empty tilemap, camera at origin pointing +Y. Every ray should miss.
	TilemapAsset tm;
	tm.width = 4;
	tm.height = 4;
	tm.cellSize = 1.f;
	auto tileset = mkShared<Tileset>();
	tileset->columns = 1;
	tileset->rows = 1;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(1);
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {16, 16}, .format = renderer::gpu::ImageFormat::Rgba8});
	tm.tileset = tileset;
	TilemapLayer layer;
	layer.name = "walls";
	layer.tiles.assign(tm.width * tm.height, owl::scene::component::g_EmptyTileIndex);
	tm.layers.push_back(std::move(layer));

	const RaycastConfig config = makeTestConfig(60.f, 8.f, 32);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 1);
	RendererRaycast::endScene();

	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.drawCalls, 1u);
	EXPECT_EQ(stats.stripeCount, 32u);
	EXPECT_EQ(stats.hitCount, 0u);
	EXPECT_EQ(stats.missCount, 32u);
	teardownRendererStack();
}

TEST(RendererRaycast, hitsWallRowFromBelow) {
	bootRendererStack();
	// Camera at world (0, 0), facing +Y. Wall row at cellY=5 in a 16×16 grid centred
	// at world origin → world Y of that row is `(15/2 - 5) * 1 = 2.5`. Looking +Y,
	// every ray within FOV that hits y=2.5 should register a hit.
	const CameraOrtho cam(0, 800, 0, 600);// rotation=0 → forward = +Y in our convention
	const TilemapAsset tm = makeCorridorTilemap();

	const RaycastConfig config = makeTestConfig(60.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 7);
	RendererRaycast::endScene();

	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.drawCalls, 1u);
	EXPECT_EQ(stats.stripeCount, 64u);
	// All 64 rays cast within ±30° of +Y should reach the row at y=5 within the
	// 16-cell budget — none should miss.
	EXPECT_EQ(stats.hitCount, 64u);
	EXPECT_EQ(stats.missCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, transparentTilesDoNotStopTheRay) {
	bootRendererStack();
	// Build a corridor where the wall row at y=5 has tile #1 (opaque) and a transparent
	// pre-wall at y=3 (tile #2 with `transparent = true`). A ray pointing straight at
	// both should register both hits — `hitCount` for one ray is 2 instead of 1.
	const CameraOrtho cam(0, 800, 0, 600);// rotation=0 → forward = +Y
	auto tileset = mkShared<Tileset>();
	tileset->columns = 4;
	tileset->rows = 4;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(tileset->tileCount());
	tileset->tiles[1].collidable = true;
	tileset->tiles[2].transparent = true;
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});

	TilemapAsset tm;
	tm.width = 16;
	tm.height = 16;
	tm.cellSize = 1.f;
	tm.tileset = tileset;
	TilemapLayer layer;
	layer.name = "walls";
	layer.tiles.assign(tm.width * tm.height, owl::scene::component::g_EmptyTileIndex);
	// Cell row Y in a 16x16 tilemap centred at origin maps to world Y = 7.5 - row,
	// so smaller cell-row Y = farther in world space when the camera looks +Y from
	// the origin. The transparent screen has to be the *nearer* cell so the DDA
	// has a chance to walk past it and reach the opaque wall behind.
	for (uint32_t x = 0; x < tm.width; ++x) {
		layer.tiles[5u * tm.width + x] = 2;// transparent screen (closer, world Y = 2.5)
		layer.tiles[3u * tm.width + x] = 1;// opaque wall behind it (farther, world Y = 4.5)
	}
	tm.layers.push_back(std::move(layer));

	// Single ray straight ahead so we can count hits exactly.
	const RaycastConfig config = makeTestConfig(60.f, 16.f, 1);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 1);
	RendererRaycast::endScene();

	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.stripeCount, 1u);
	// Two stripes drawn for that single ray (transparent + opaque).
	EXPECT_EQ(stats.hitCount, 2u);
	EXPECT_EQ(stats.missCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, transparentBudgetCapsTheStack) {
	bootRendererStack();
	// 16 transparent tiles in a row, no opaque behind. The DDA should bail at the
	// `kMaxTransparentHits` budget (8) so a pathological scene doesn't blow up.
	const CameraOrtho cam(0, 800, 0, 600);
	auto tileset = mkShared<Tileset>();
	tileset->columns = 4;
	tileset->rows = 4;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(tileset->tileCount());
	tileset->tiles[1].transparent = true;
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});

	TilemapAsset tm;
	tm.width = 16;
	tm.height = 16;
	tm.cellSize = 1.f;
	tm.tileset = tileset;
	TilemapLayer layer;
	layer.name = "walls";
	layer.tiles.assign(tm.width * tm.height, owl::scene::component::g_EmptyTileIndex);
	// 12 transparent rows in front of the camera at y=2..14.
	for (uint32_t y = 2; y < 14; ++y)
		for (uint32_t x = 0; x < tm.width; ++x) layer.tiles[y * tm.width + x] = 1;
	tm.layers.push_back(std::move(layer));

	const RaycastConfig config = makeTestConfig(60.f, 32.f, 1);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 1);
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.stripeCount, 1u);
	// Cap at kMaxTransparentHits = 8 — never more.
	EXPECT_LE(stats.hitCount, 8u);
	teardownRendererStack();
}

TEST(RendererRaycast, opaqueWallStopsRayLikeBeforePR3) {
	bootRendererStack();
	// Sanity check: with no transparent tiles the new multi-hit path collapses to
	// the legacy single-hit behaviour (one stripe per ray that hit).
	const CameraOrtho cam(0, 800, 0, 600);
	const TilemapAsset tm = makeCorridorTilemap();// row 5 fully opaque
	const RaycastConfig config = makeTestConfig(60.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 1);
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.stripeCount, 64u);
	EXPECT_EQ(stats.hitCount, 64u);// exactly one per ray
	teardownRendererStack();
}

TEST(RendererRaycast, drawSpritesEmptySpanIsNoOp) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawSprites({});
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.spriteCount, 0u);
	EXPECT_EQ(stats.spriteStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawSpritesCullsBehindCamera) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);// rotation=0 → forward = +Y
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RaycastSpriteData behind{};
	behind.worldPosition = {0.f, -2.f};// camera at origin facing +Y → -2 is behind
	behind.worldSize = {1.f, 1.f};
	behind.texture = makeSpriteTexture();
	std::array sprites{behind};
	RendererRaycast::drawSprites(std::span<const RaycastSpriteData>(sprites.data(), sprites.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.spriteCount, 0u);
	EXPECT_EQ(stats.spriteStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawSpritesEmitsStripesWhenInFront) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RaycastSpriteData front{};
	front.worldPosition = {0.f, 3.f};// 3 cells ahead, no walls anywhere
	front.worldSize = {1.f, 1.f};
	front.texture = makeSpriteTexture();
	std::array sprites{front};
	RendererRaycast::drawSprites(std::span<const RaycastSpriteData>(sprites.data(), sprites.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.spriteCount, 1u);
	EXPECT_GT(stats.spriteStripeCount, 0u);
	EXPECT_EQ(stats.spriteOccludedCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawSpritesOccludedByWall) {
	bootRendererStack();
	// Wall row at cellY=5 (worldY=2.5). Place a sprite *behind* the wall
	// (at worldY=4) so all stripes that would land in its column should be
	// occluded — `spriteCount` stays 0 because every column was rejected.
	const CameraOrtho cam(0, 800, 0, 600);
	const TilemapAsset tm = makeCorridorTilemap();
	const RaycastConfig config = makeTestConfig(60.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 1);
	RaycastSpriteData behindWall{};
	behindWall.worldPosition = {0.f, 4.f};
	behindWall.worldSize = {1.f, 1.f};
	behindWall.texture = makeSpriteTexture();
	std::array sprites{behindWall};
	RendererRaycast::drawSprites(std::span<const RaycastSpriteData>(sprites.data(), sprites.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.spriteCount, 0u);
	EXPECT_EQ(stats.spriteStripeCount, 0u);
	EXPECT_GT(stats.spriteOccludedCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawSpritesBeyondMaxDistanceIsCulled) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 5.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RaycastSpriteData far{};
	far.worldPosition = {0.f, 10.f};// 10 > maxDistance 5
	far.worldSize = {1.f, 1.f};
	far.texture = makeSpriteTexture();
	std::array sprites{far};
	RendererRaycast::drawSprites(std::span<const RaycastSpriteData>(sprites.data(), sprites.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.spriteCount, 0u);
	EXPECT_EQ(stats.spriteStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawSpritesWithoutTextureIsSkipped) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RaycastSpriteData noTex{};
	noTex.worldPosition = {0.f, 3.f};
	noTex.worldSize = {1.f, 1.f};
	noTex.texture = nullptr;
	std::array sprites{noTex};
	RendererRaycast::drawSprites(std::span<const RaycastSpriteData>(sprites.data(), sprites.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.spriteCount, 0u);
	EXPECT_EQ(stats.spriteStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, statsResetClearsAllCounters) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const TilemapAsset tm = makeCorridorTilemap();
	const RaycastConfig config = makeTestConfig(60.f, 16.f, 32);
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 0);
	RendererRaycast::endScene();
	EXPECT_GT(RendererRaycast::getStats().stripeCount, 0u);
	RendererRaycast::resetStats();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.drawCalls, 0u);
	EXPECT_EQ(stats.stripeCount, 0u);
	EXPECT_EQ(stats.hitCount, 0u);
	EXPECT_EQ(stats.missCount, 0u);
	EXPECT_EQ(stats.spriteCount, 0u);
	EXPECT_EQ(stats.spriteStripeCount, 0u);
	EXPECT_EQ(stats.spriteOccludedCount, 0u);
	EXPECT_EQ(stats.dynamicWallCount, 0u);
	EXPECT_EQ(stats.dynamicWallStripeCount, 0u);
	EXPECT_EQ(stats.doorCount, 0u);
	EXPECT_EQ(stats.doorStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawDynamicWallsEmptySpanIsNoOp) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawDynamicWalls(std::span<const RaycastDynamicWallData>{});
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.dynamicWallCount, 0u);
	EXPECT_EQ(stats.dynamicWallStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawDynamicWallsEmitsStripesWhenInFront) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RaycastDynamicWallData door{};
	door.worldCenter = {0.f, 3.f};// 3 cells ahead, 1-cell box, clear FOV
	door.halfExtent = {0.5f, 0.5f};
	door.texture = makeSpriteTexture();
	std::array walls{door};
	RendererRaycast::drawDynamicWalls(std::span<const RaycastDynamicWallData>(walls.data(), walls.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.dynamicWallCount, 1u);
	EXPECT_GT(stats.dynamicWallStripeCount, 0u);
}

TEST(RendererRaycast, drawDynamicWallsOccludedByCloserStaticWall) {
	// Static corridor wall at cellY=5 (worldY=~2.5) blocks every ray; the door
	// sitting at worldY=4 is fully behind and should produce no stripes.
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const TilemapAsset tm = makeCorridorTilemap();
	const RaycastConfig config = makeTestConfig(60.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 1);
	RaycastDynamicWallData door{};
	door.worldCenter = {0.f, 4.f};
	door.halfExtent = {0.5f, 0.5f};
	door.texture = makeSpriteTexture();
	std::array walls{door};
	RendererRaycast::drawDynamicWalls(std::span<const RaycastDynamicWallData>(walls.data(), walls.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.dynamicWallCount, 0u);
	EXPECT_EQ(stats.dynamicWallStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawDynamicWallsBeyondMaxDistanceIsCulled) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 4.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RaycastDynamicWallData door{};
	door.worldCenter = {0.f, 10.f};// 10 > maxDistance 4
	door.halfExtent = {0.5f, 0.5f};
	door.texture = makeSpriteTexture();
	std::array walls{door};
	RendererRaycast::drawDynamicWalls(std::span<const RaycastDynamicWallData>(walls.data(), walls.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.dynamicWallCount, 0u);
	EXPECT_EQ(stats.dynamicWallStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawDynamicWallsWithoutTextureIsSkipped) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RaycastDynamicWallData door{};
	door.worldCenter = {0.f, 3.f};
	door.halfExtent = {0.5f, 0.5f};
	door.texture = nullptr;
	std::array walls{door};
	RendererRaycast::drawDynamicWalls(std::span<const RaycastDynamicWallData>(walls.data(), walls.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.dynamicWallCount, 0u);
	EXPECT_EQ(stats.dynamicWallStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawDoorsEmptySpanIsNoOp) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawDoors(std::span<const owl::renderer::RaycastDoorData>{});
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.doorCount, 0u);
	EXPECT_EQ(stats.doorStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawDoorsClosedShowsLateralsAndPlate) {
	// Closed door (plateOffset = 0) at 3 cells ahead: the plate sits at the cell
	// centre so a ray cast head-on through the cell should hit it. Plus the two
	// laterals are always there, hit by rays at the cell edges.
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 128);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	owl::renderer::RaycastDoorData door{};
	door.cellCenter = {0.f, 3.f};
	door.openingDirection = 2;// East
	door.plateOffset = 0.f;// closed
	door.faceTexture = makeSpriteTexture();
	door.lateralTexture = makeSpriteTexture();
	std::array doors{door};
	RendererRaycast::drawDoors(std::span<const owl::renderer::RaycastDoorData>(doors.data(), doors.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.doorCount, 1u);
	EXPECT_GT(stats.doorStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawDoorsOpenStillRendersLaterals) {
	// Open door (plateOffset = cellSize): the plate is hidden inside the pocket,
	// but the two static laterals are still drawn — so the door must still
	// contribute at least some stripes (just not the plate stripes).
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 128);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	owl::renderer::RaycastDoorData door{};
	door.cellCenter = {0.f, 3.f};
	door.openingDirection = 2;// East
	door.plateOffset = 1.f;// fully open — plate hidden in pocket
	door.faceTexture = makeSpriteTexture();
	door.lateralTexture = makeSpriteTexture();
	std::array doors{door};
	RendererRaycast::drawDoors(std::span<const owl::renderer::RaycastDoorData>(doors.data(), doors.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.doorCount, 1u);
	EXPECT_GT(stats.doorStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, drawDoorsWithoutTextureIsSkipped) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	owl::renderer::RaycastDoorData door{};
	door.cellCenter = {0.f, 3.f};
	door.faceTexture = nullptr;
	door.lateralTexture = nullptr;
	std::array doors{door};
	RendererRaycast::drawDoors(std::span<const owl::renderer::RaycastDoorData>(doors.data(), doors.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.doorCount, 0u);
	EXPECT_EQ(stats.doorStripeCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, fogDisabledByDefault) {
	// Default config has fogEnd == fogStart == 0 — no fog should be applied.
	// We exercise the path with a wall pass and just verify the renderer
	// doesn't crash and still emits stripes.
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const TilemapAsset tm = makeCorridorTilemap();
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 0);
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_GT(stats.hitCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, fogConfiguredAppliesWithoutCrash) {
	// Configure a fog range that covers the corridor wall depth; the test only
	// verifies the renderer survives the fog math (the actual tint blend is
	// emitted to Renderer2D and isn't directly observable in the stats).
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const TilemapAsset tm = makeCorridorTilemap();
	RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	config.fogColor = {0.f, 0.f, 0.f, 1.f};
	config.fogStart = 1.f;
	config.fogEnd = 8.f;
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	RendererRaycast::drawTilemapWalls(tm, math::Transform{}, 0);
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_GT(stats.hitCount, 0u);
	teardownRendererStack();
}

TEST(RendererRaycast, backdropTextureEmitsScanlines) {
	// When the config carries floor / ceiling textures the renderer should
	// emit one quad per screen row instead of two solid full-screen quads.
	// `backdropScanlineCount` counts both halves combined.
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	auto cfg = makeTestConfig(75.f, 16.f, 64);
	cfg.floorTexture = makeSpriteTexture();
	cfg.ceilingTexture = makeSpriteTexture();
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, cfg);
	// Drawing an empty tilemap still triggers the backdrop emit on first use;
	// route that explicitly via a sprite pass since the empty tilemap fast-path
	// returns before the backdrop is needed.
	owl::renderer::RaycastSpriteData sprite{};
	sprite.worldPosition = {0.f, 3.f};
	sprite.worldSize = {1.f, 1.f};
	sprite.texture = makeSpriteTexture();
	std::array sprites{sprite};
	RendererRaycast::drawSprites(std::span<const owl::renderer::RaycastSpriteData>(sprites.data(), sprites.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_GT(stats.backdropScanlineCount, 0u);
	// Roughly one scanline per row across both halves (600 = floor + ceiling).
	EXPECT_LE(stats.backdropScanlineCount, 600u);
	teardownRendererStack();
}

TEST(RendererRaycast, backdropFallsBackToSolidWhenNoTexture) {
	// With no textures the backdrop is the original 2-quad solid fill — the
	// scanline counter stays at zero.
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const RaycastConfig config = makeTestConfig(75.f, 16.f, 64);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	owl::renderer::RaycastSpriteData sprite{};
	sprite.worldPosition = {0.f, 3.f};
	sprite.worldSize = {1.f, 1.f};
	sprite.texture = makeSpriteTexture();
	std::array sprites{sprite};
	RendererRaycast::drawSprites(std::span<const owl::renderer::RaycastSpriteData>(sprites.data(), sprites.size()));
	RendererRaycast::endScene();
	const auto stats = RendererRaycast::getStats();
	EXPECT_EQ(stats.backdropScanlineCount, 0u);
	teardownRendererStack();
}
