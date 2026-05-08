/**
 * @file RendererRaycast_test.cpp
 * @author Silmaen
 * @date 04/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/Renderer.h>
#include <core/Application.h>
#include <renderer/CameraOrtho.h>
#include <renderer/RendererRaycast.h>
#include <renderer/RenderLayerFactory.h>
#include <scene/Tileset.h>
#include <scene/component/Tilemap.h>

#include "renderer/RendererRaycastLayer.h"

using namespace owl;
using owl::renderer::CameraOrtho;
using owl::renderer::Renderer;
using owl::renderer::RaycastConfig;
using owl::renderer::RendererRaycast;
using owl::renderer::RendererRaycastLayer;
using owl::renderer::RenderLayerFactory;
using owl::scene::Tileset;
using owl::scene::component::Tilemap;
using owl::scene::component::TilemapLayer;

namespace {
/// Build a simple `Tilemap` filled with a single-row corridor of walls along
/// the +X axis at cellY = 5, 16 cells wide × 16 cells tall, cellSize = 1.
auto makeCorridorTilemap() -> Tilemap {
	Tilemap tm;
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
	for (uint32_t x = 0; x < tm.width; ++x)
		layer.tiles[5u * tm.width + x] = 1;
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
	const RaycastConfig config{.fovDegrees = 75.f, .maxDistance = 16.f, .numRays = 64};
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(cam, {800, 600}, config);
	const Tilemap empty;
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
	Tilemap tm;
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

	const RaycastConfig config{.fovDegrees = 60.f, .maxDistance = 8.f, .numRays = 32};
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
	const Tilemap tm = makeCorridorTilemap();

	const RaycastConfig config{.fovDegrees = 60.f, .maxDistance = 16.f, .numRays = 64};
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

TEST(RendererRaycast, statsResetClearsAllCounters) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	const Tilemap tm = makeCorridorTilemap();
	const RaycastConfig config{.fovDegrees = 60.f, .maxDistance = 16.f, .numRays = 32};
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
	teardownRendererStack();
}
