/**
 * @file RendererTilemap_test.cpp
 * @author Silmaen
 * @date 02/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/CameraOrtho.h>
#include <renderer/Renderer.h>
#include <renderer/RendererTilemap.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>
#include <scene/component/Tilemap.h>

using namespace owl;
using owl::renderer::CameraOrtho;
using owl::renderer::Renderer;
using owl::renderer::RendererTilemap;
using owl::scene::TilemapAsset;
using owl::scene::Tileset;
using owl::scene::component::TilemapLayer;

namespace {

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

auto makeTileset() -> shared<Tileset> {
	auto tileset = mkShared<Tileset>();
	tileset->columns = 4;
	tileset->rows = 4;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(tileset->tileCount());
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});
	return tileset;
}

auto makeLayer(const TilemapAsset& iAsset, const std::string& iName, const bool iVisible) -> TilemapLayer {
	TilemapLayer layer;
	layer.name = iName;
	layer.visible = iVisible;
	layer.parallax = math::vec2{1.f, 1.f};
	layer.tiles.assign(iAsset.width * iAsset.height, owl::scene::component::g_EmptyTileIndex);
	return layer;
}

}// namespace

TEST(RendererTilemap, initIsIdempotentAndShutdownSafe) {
	bootRendererStack();
	RendererTilemap::init();
	RendererTilemap::init();
	RendererTilemap::shutdown();
	RendererTilemap::shutdown();
	teardownRendererStack();
}

TEST(RendererTilemap, beginSceneResetsStats) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	RendererTilemap::beginScene(cam);
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.drawCallCount, 0u);
	EXPECT_EQ(stats.instanceCount, 0u);
	EXPECT_EQ(stats.layerCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, drawTilemapCountsNonEmptyCells) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	RendererTilemap::beginScene(cam);

	TilemapAsset tm;
	tm.width = 3;
	tm.height = 3;
	tm.cellSize = 1.f;
	tm.tileset = makeTileset();
	TilemapLayer layer = makeLayer(tm, "ground", true);
	layer.tiles[0] = 0;
	layer.tiles[4] = 1;
	layer.tiles[8] = 2;
	tm.layers.push_back(std::move(layer));

	RendererTilemap::drawTilemap(tm, math::Transform{}, /*iEntityId=*/7);
	RendererTilemap::flushPending();
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 1u);
	EXPECT_EQ(stats.drawCallCount, 1u);
	EXPECT_EQ(stats.instanceCount, 3u);
	teardownRendererStack();
}

TEST(RendererTilemap, invisibleLayerIsWalkedButNotDrawn) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	RendererTilemap::beginScene(cam);

	TilemapAsset tm;
	tm.width = 2;
	tm.height = 2;
	tm.tileset = makeTileset();
	TilemapLayer hidden = makeLayer(tm, "hidden", false);
	hidden.tiles[0] = 1;
	hidden.tiles[1] = 1;
	tm.layers.push_back(std::move(hidden));

	RendererTilemap::drawTilemap(tm, math::Transform{}, /*iEntityId=*/-1);
	RendererTilemap::flushPending();
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 1u);
	EXPECT_EQ(stats.drawCallCount, 0u);
	EXPECT_EQ(stats.instanceCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, emptyVisibleLayerEmitsNoDrawCall) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	RendererTilemap::beginScene(cam);

	TilemapAsset tm;
	tm.width = 4;
	tm.height = 4;
	tm.tileset = makeTileset();
	tm.layers.push_back(makeLayer(tm, "empty", true));

	RendererTilemap::drawTilemap(tm, math::Transform{}, /*iEntityId=*/-1);
	RendererTilemap::flushPending();
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 1u);
	EXPECT_EQ(stats.drawCallCount, 0u);
	EXPECT_EQ(stats.instanceCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, multiLayerAccumulatesStats) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	RendererTilemap::beginScene(cam);

	TilemapAsset tm;
	tm.width = 2;
	tm.height = 2;
	tm.tileset = makeTileset();
	TilemapLayer ground = makeLayer(tm, "ground", true);
	ground.tiles[0] = 0;
	ground.tiles[1] = 0;
	ground.tiles[2] = 0;
	ground.tiles[3] = 0;
	tm.layers.push_back(std::move(ground));
	TilemapLayer decor = makeLayer(tm, "decor", true);
	decor.tiles[3] = 5;
	tm.layers.push_back(std::move(decor));

	RendererTilemap::drawTilemap(tm, math::Transform{}, /*iEntityId=*/-1);
	RendererTilemap::flushPending();
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 2u);
	// All visible layers combine into a single instanced drawcall.
	EXPECT_EQ(stats.drawCallCount, 1u);
	EXPECT_EQ(stats.instanceCount, 5u);
	teardownRendererStack();
}

TEST(RendererTilemap, missingTilesetEmitsNothing) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	RendererTilemap::beginScene(cam);

	TilemapAsset tm;
	tm.width = 2;
	tm.height = 2;
	tm.layers.push_back(makeLayer(tm, "ground", true));

	RendererTilemap::drawTilemap(tm, math::Transform{}, /*iEntityId=*/-1);
	RendererTilemap::flushPending();
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 0u);
	EXPECT_EQ(stats.drawCallCount, 0u);
	EXPECT_EQ(stats.instanceCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, multipleEntitiesCombineIntoOneDrawcall) {
	bootRendererStack();
	const CameraOrtho cam(0, 800, 0, 600);
	RendererTilemap::beginScene(cam);

	TilemapAsset first;
	first.width = 2;
	first.height = 2;
	first.tileset = makeTileset();
	TilemapLayer firstLayer = makeLayer(first, "a", true);
	firstLayer.tiles[0] = 0;
	firstLayer.tiles[1] = 0;
	first.layers.push_back(std::move(firstLayer));

	TilemapAsset second;
	second.width = 2;
	second.height = 2;
	second.tileset = makeTileset();
	TilemapLayer secondLayer = makeLayer(second, "b", true);
	secondLayer.tiles[3] = 1;
	second.layers.push_back(std::move(secondLayer));

	RendererTilemap::drawTilemap(first, math::Transform{}, /*iEntityId=*/1);
	RendererTilemap::drawTilemap(second, math::Transform{}, /*iEntityId=*/2);
	RendererTilemap::flushPending();

	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 2u);
	// Two tilemap entities with distinct tilesets combine into one drawcall.
	EXPECT_EQ(stats.drawCallCount, 1u);
	EXPECT_EQ(stats.instanceCount, 3u);
	teardownRendererStack();
}
