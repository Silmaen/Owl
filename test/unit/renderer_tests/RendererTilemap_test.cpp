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

#include <cstdint>
#include <numbers>
#include <string>
#include <utility>

using namespace owl;
using owl::renderer::CameraOrtho;
using owl::renderer::Renderer;
using owl::renderer::RendererTilemap;
using owl::scene::TilemapAsset;
using owl::scene::Tileset;
using owl::scene::component::TilemapLayer;

namespace {

// Bytes of one chunk instance (`{packed cell, tile index}`).
constexpr uint64_t g_instanceBytes = 8;

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
	layer.resize({iAsset.width, iAsset.height});
	return layer;
}

auto makeFilledMap(const uint32_t iSize) -> TilemapAsset {
	TilemapAsset tm;
	tm.width = iSize;
	tm.height = iSize;
	tm.tileset = makeTileset();
	TilemapLayer layer = makeLayer(tm, "ground", true);
	layer.fill(1);
	tm.layers.push_back(std::move(layer));
	return tm;
}

void drawFrame(const renderer::Camera& iCamera, const TilemapAsset& iAsset, const math::Transform& iTransform = {}) {
	RendererTilemap::beginScene(iCamera);
	RendererTilemap::drawTilemap(iAsset, iTransform, /*iEntityId=*/1);
	RendererTilemap::flushPending();
}

}// namespace

TEST(RendererTilemap, initIsIdempotentAndShutdownSafe) {
	bootRendererStack();
	EXPECT_NO_THROW(RendererTilemap::init());
	EXPECT_NO_THROW(RendererTilemap::init());
	EXPECT_NO_THROW(RendererTilemap::shutdown());
	EXPECT_NO_THROW(RendererTilemap::shutdown());
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
	EXPECT_EQ(stats.culledChunkCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, drawTilemapCountsNonEmptyCells) {
	bootRendererStack();
	const CameraOrtho cam(-10, 10, -10, 10);

	TilemapAsset tm;
	tm.width = 3;
	tm.height = 3;
	tm.cellSize = 1.f;
	tm.tileset = makeTileset();
	TilemapLayer layer = makeLayer(tm, "ground", true);
	layer.setTile(0, 0, 0);
	layer.setTile(1, 1, 1);
	layer.setTile(2, 2, 2);
	tm.layers.push_back(std::move(layer));

	drawFrame(cam, tm);
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 1u);
	EXPECT_EQ(stats.drawCallCount, 1u);
	EXPECT_EQ(stats.instanceCount, 3u);
	teardownRendererStack();
}

TEST(RendererTilemap, invisibleLayerIsWalkedButNotDrawn) {
	bootRendererStack();
	const CameraOrtho cam(-10, 10, -10, 10);

	TilemapAsset tm;
	tm.width = 2;
	tm.height = 2;
	tm.tileset = makeTileset();
	TilemapLayer hidden = makeLayer(tm, "hidden", false);
	hidden.setTile(0, 0, 1);
	hidden.setTile(1, 0, 1);
	tm.layers.push_back(std::move(hidden));

	drawFrame(cam, tm);
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 1u);
	EXPECT_EQ(stats.drawCallCount, 0u);
	EXPECT_EQ(stats.instanceCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, emptyVisibleLayerEmitsNoDrawCall) {
	bootRendererStack();
	const CameraOrtho cam(-10, 10, -10, 10);

	TilemapAsset tm;
	tm.width = 4;
	tm.height = 4;
	tm.tileset = makeTileset();
	tm.layers.push_back(makeLayer(tm, "empty", true));

	drawFrame(cam, tm);
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 1u);
	EXPECT_EQ(stats.drawCallCount, 0u);
	EXPECT_EQ(stats.instanceCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, eachLayerDrawsItsOwnChunks) {
	bootRendererStack();
	const CameraOrtho cam(-10, 10, -10, 10);

	TilemapAsset tm;
	tm.width = 2;
	tm.height = 2;
	tm.tileset = makeTileset();
	TilemapLayer ground = makeLayer(tm, "ground", true);
	ground.fill(0);
	tm.layers.push_back(std::move(ground));
	TilemapLayer decor = makeLayer(tm, "decor", true);
	decor.setTile(1, 1, 5);
	tm.layers.push_back(std::move(decor));

	drawFrame(cam, tm);
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 2u);
	EXPECT_EQ(stats.drawCallCount, 2u);
	EXPECT_EQ(stats.instanceCount, 5u);
	teardownRendererStack();
}

TEST(RendererTilemap, missingTilesetEmitsNothing) {
	bootRendererStack();
	const CameraOrtho cam(-10, 10, -10, 10);

	TilemapAsset tm;
	tm.width = 2;
	tm.height = 2;
	tm.layers.push_back(makeLayer(tm, "ground", true));

	drawFrame(cam, tm);
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 0u);
	EXPECT_EQ(stats.drawCallCount, 0u);
	EXPECT_EQ(stats.instanceCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, multipleEntitiesDrawInQueueOrder) {
	bootRendererStack();
	const CameraOrtho cam(-10, 10, -10, 10);
	RendererTilemap::beginScene(cam);

	TilemapAsset first;
	first.width = 2;
	first.height = 2;
	first.tileset = makeTileset();
	TilemapLayer firstLayer = makeLayer(first, "a", true);
	firstLayer.setTile(0, 0, 0);
	firstLayer.setTile(1, 0, 0);
	first.layers.push_back(std::move(firstLayer));

	TilemapAsset second;
	second.width = 2;
	second.height = 2;
	second.tileset = makeTileset();
	TilemapLayer secondLayer = makeLayer(second, "b", true);
	secondLayer.setTile(1, 1, 1);
	second.layers.push_back(std::move(secondLayer));

	RendererTilemap::drawTilemap(first, math::Transform{}, /*iEntityId=*/1);
	RendererTilemap::drawTilemap(second, math::Transform{}, /*iEntityId=*/2);
	RendererTilemap::flushPending();

	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.layerCount, 2u);
	EXPECT_EQ(stats.drawCallCount, 2u);
	EXPECT_EQ(stats.instanceCount, 3u);
	teardownRendererStack();
}

TEST(RendererTilemap, staticMapUploadsOnlyOnce) {
	bootRendererStack();
	const auto tm = makeFilledMap(64);

	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);
	const auto first = RendererTilemap::getPrepareStatistics();
	EXPECT_EQ(first.builtChunkCount, 4u);
	EXPECT_GE(first.uploadedBytes, uint64_t{64} * 64 * g_instanceBytes);

	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);
	const auto second = RendererTilemap::getPrepareStatistics();
	EXPECT_EQ(second.builtChunkCount, 0u);
	EXPECT_EQ(second.uploadedBytes, 0u);
	EXPECT_EQ(RendererTilemap::getCachedAssetCount(), 1u);
	teardownRendererStack();
}

TEST(RendererTilemap, oneCellEditUploadsOneChunk) {
	bootRendererStack();
	auto tm = makeFilledMap(64);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);

	tm.setTile(0, 40, 33, scene::component::g_EmptyTileIndex);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);
	const auto stats = RendererTilemap::getPrepareStatistics();
	EXPECT_EQ(stats.builtChunkCount, 1u);
	EXPECT_EQ(stats.lateBuiltChunkCount, 0u);
	EXPECT_EQ(stats.uploadedBytes, uint64_t{1023} * g_instanceBytes);
	teardownRendererStack();
}

TEST(RendererTilemap, unpreparedAssetIsBuiltLate) {
	bootRendererStack();
	const CameraOrtho cam(-40, 40, -40, 40);
	const auto tm = makeFilledMap(40);
	RendererTilemap::beginPrepare();
	drawFrame(cam, tm);
	const auto stats = RendererTilemap::getPrepareStatistics();
	EXPECT_EQ(stats.builtChunkCount, 4u);
	EXPECT_EQ(stats.lateBuiltChunkCount, 4u);
	EXPECT_EQ(RendererTilemap::getStatistics().instanceCount, 1600u);
	teardownRendererStack();
}

TEST(RendererTilemap, largeMapIsNotTruncated) {
	bootRendererStack();
	const CameraOrtho cam(-200, 200, -200, 200);
	const auto tm = makeFilledMap(256);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);
	drawFrame(cam, tm);
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.instanceCount, 256u * 256u);
	EXPECT_EQ(stats.drawCallCount, 64u);
	EXPECT_EQ(stats.culledChunkCount, 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, chunksOutsideTheViewAreCulled) {
	bootRendererStack();
	const CameraOrtho cam(0.25f, 9.75f, 0.25f, 9.75f);
	const auto tm = makeFilledMap(128);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);
	drawFrame(cam, tm);
	const auto stats = RendererTilemap::getStatistics();
	EXPECT_EQ(stats.drawCallCount, 1u);
	EXPECT_EQ(stats.instanceCount, 1024u);
	EXPECT_EQ(stats.culledChunkCount, 15u);
	teardownRendererStack();
}

TEST(RendererTilemap, entityTransformMovesTheChunks) {
	bootRendererStack();
	const CameraOrtho cam(0.25f, 9.75f, 0.25f, 9.75f);
	const auto tm = makeFilledMap(128);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);

	math::Transform far;
	far.translation() = {1000.f, 0.f, 0.f};
	drawFrame(cam, tm, far);
	EXPECT_EQ(RendererTilemap::getStatistics().drawCallCount, 0u);
	EXPECT_EQ(RendererTilemap::getStatistics().culledChunkCount, 16u);

	math::Transform scaled;
	scaled.scale() = {0.1f, 0.1f, 1.f};
	drawFrame(cam, tm, scaled);
	EXPECT_EQ(RendererTilemap::getStatistics().drawCallCount, 4u);

	math::Transform rotated;
	rotated.rotation() = {0.f, 0.f, std::numbers::pi_v<float>};
	drawFrame(cam, tm, rotated);
	EXPECT_EQ(RendererTilemap::getStatistics().drawCallCount, 1u);
	EXPECT_EQ(RendererTilemap::getStatistics().culledChunkCount, 15u);
	teardownRendererStack();
}

TEST(RendererTilemap, parallaxZeroFollowsTheCamera) {
	bootRendererStack();
	CameraOrtho cam(0.25f, 9.75f, 0.25f, 9.75f);
	cam.setPosition({5000.f, 0.f, 0.f});
	auto tm = makeFilledMap(128);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);

	drawFrame(cam, tm);
	EXPECT_EQ(RendererTilemap::getStatistics().drawCallCount, 0u);

	tm.layers[0].parallax = {0.f, 0.f};
	drawFrame(cam, tm);
	EXPECT_EQ(RendererTilemap::getStatistics().drawCallCount, 1u);
	EXPECT_EQ(RendererTilemap::getStatistics().instanceCount, 1024u);
	teardownRendererStack();
}

TEST(RendererTilemap, unusedAssetsAreEvicted) {
	bootRendererStack();
	const auto tm = makeFilledMap(8);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);
	EXPECT_EQ(RendererTilemap::getCachedAssetCount(), 1u);
	for (uint64_t i = 0; i < uint64_t{RendererTilemap::cacheEvictionPrepares}; ++i) RendererTilemap::beginPrepare();
	EXPECT_EQ(RendererTilemap::getCachedAssetCount(), 1u);
	RendererTilemap::beginPrepare();
	EXPECT_EQ(RendererTilemap::getCachedAssetCount(), 0u);

	RendererTilemap::prepareTilemap(tm);
	RendererTilemap::clearCache();
	EXPECT_EQ(RendererTilemap::getCachedAssetCount(), 0u);
	teardownRendererStack();
}

TEST(RendererTilemap, resizedLayerIsRebuilt) {
	bootRendererStack();
	auto tm = makeFilledMap(32);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);
	tm.resize(40, 32);
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(tm);
	EXPECT_EQ(RendererTilemap::getPrepareStatistics().builtChunkCount, 2u);
	teardownRendererStack();
}
