/**
 * @file RendererIsometric_test.cpp
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/linAlgebra.h>
#include <renderer/IsometricObjects.h>
#include <renderer/RenderLayerFactory.h>
#include <renderer/Renderer.h>
#include <renderer/RendererIsometric.h>
#include <renderer/RendererIsometricLayer.h>
#include <scene/TilemapAsset.h>
#include <scene/TilemapTerrain.h>
#include <scene/Tileset.h>
#include <scene/component/Tilemap.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl;
using owl::renderer::CameraOrtho;
using owl::renderer::getBackToFrontOrder;
using owl::renderer::getSpriteScreenRect;
using owl::renderer::IsometricBillboard;
using owl::renderer::IsometricCamera;
using owl::renderer::IsometricConfig;
using owl::renderer::IsometricObjectSet;
using owl::renderer::IsometricSprite;
using owl::renderer::packColor;
using owl::renderer::pickIsometricCell;
using owl::renderer::Renderer;
using owl::renderer::RendererIsometric;
using owl::renderer::RendererIsometricLayer;
using owl::renderer::RenderLayerFactory;
using owl::renderer::screenToWorld;
using owl::renderer::worldToScreen;

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

// Viewport pixel (top left origin, Y down) of a world point through a view-projection in a clip-space convention.
auto projectToPixel(const math::mat4& iViewProjection, const math::vec3& iWorld, const math::vec2ui& iViewport,
					const CameraOrtho::ClipSpace iClipSpace) -> math::vec2 {
	const math::vec4 clip = iViewProjection * math::vec4{iWorld.x(), iWorld.y(), iWorld.z(), 1.f};
	const float ndcX = clip.x() / clip.w();
	const float ndcY = clip.y() / clip.w();
	const float downY = iClipSpace == CameraOrtho::ClipSpace::Vulkan ? ndcY : -ndcY;
	return {(ndcX + 1.f) * 0.5f * static_cast<float>(iViewport.x()),
			(downY + 1.f) * 0.5f * static_cast<float>(iViewport.y())};
}

auto ndcDepth(const math::mat4& iViewProjection, const math::vec3& iWorld) -> float {
	const math::vec4 clip = iViewProjection * math::vec4{iWorld.x(), iWorld.y(), iWorld.z(), 1.f};
	return clip.z() / clip.w();
}

auto makeCamera() -> IsometricCamera {
	IsometricCamera camera;
	camera.setViewportSize({800u, 600u});
	camera.setTarget({4.f, 3.f, 0.f});
	return camera;
}

auto makeIsometricMap() -> scene::TilemapAsset {
	scene::TilemapAsset map;
	map.width = 8;
	map.height = 8;
	map.tileset = mkShared<scene::Tileset>();
	map.tileset->kind = scene::Tileset::Kind::Isometric;
	map.tileset->columns = 2;
	map.tileset->rows = 2;
	map.tileset->tileWidth = 64;
	map.tileset->tileHeight = 32;
	map.tileset->tiles.resize(map.tileset->tileCount());
	map.tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {128, 64}, .format = renderer::gpu::ImageFormat::Rgba8});
	auto& ground = map.addLayer("ground");
	ground.fill(0);
	auto& roads = map.addLayer("roads");
	roads.setTile(1, 1, 3);
	return map;
}

}// namespace

TEST(RendererIsometric, factoryRegistersTypeKey) {
	bootRendererStack();
	EXPECT_TRUE(RenderLayerFactory::hasType("RendererIsometric"));
	const auto layer = RenderLayerFactory::create("RendererIsometric", "iso");
	ASSERT_NE(layer, nullptr);
	EXPECT_STREQ(layer->getTypeKey(), "RendererIsometric");
	EXPECT_EQ(layer->getName(), "iso");
	EXPECT_TRUE(layer->drawsOwnEntities());
	teardownRendererStack();
}

TEST(RendererIsometric, defaultConfigIsTwoToOne) {
	const IsometricConfig cfg;
	EXPECT_EQ(cfg.tileSize.x(), 64u);
	EXPECT_EQ(cfg.tileSize.y(), 32u);
	EXPECT_NEAR(renderer::getIsometricPitch(cfg), std::numbers::pi_v<float> / 6.f, 1e-6f);
}

TEST(RendererIsometric, applyConfigParsesYaml) {
	RendererIsometricLayer layer{"iso"};
	layer.applyConfig("{TileWidth: 128, TileHeight: 64, ZStep: 24, Zoom: 9}");
	EXPECT_EQ(layer.getConfig().tileSize.x(), 128u);
	EXPECT_EQ(layer.getConfig().tileSize.y(), 64u);
	EXPECT_FLOAT_EQ(layer.getConfig().zStep, 24.f);
	EXPECT_EQ(layer.getZoomLevel(), int32_t{IsometricCamera::maxZoomLevel});
}

TEST(RendererIsometric, applyConfigKeepsDefaultsOnEmpty) {
	RendererIsometricLayer layer{"iso"};
	layer.applyConfig("");
	layer.applyConfig("not: [a, map");
	EXPECT_EQ(layer.getConfig().tileSize.x(), 64u);
	EXPECT_EQ(layer.getConfig().tileSize.y(), 32u);
	EXPECT_EQ(layer.getZoomLevel(), 0);
}

TEST(RendererIsometric, worldScreenRoundTrip) {
	IsometricConfig cfg;
	cfg.origin = math::vec2{400.f, 50.f};
	for (const auto& world: {math::vec3{0.f, 0.f, 0.f}, math::vec3{3.f, 2.f, 1.f}, math::vec3{-4.f, 7.f, -2.f},
							 math::vec3{12.5f, -3.5f, 4.f}}) {
		const math::vec2 screen = worldToScreen(world, cfg);
		const math::vec3 back = screenToWorld(screen, world.z(), cfg);
		EXPECT_NEAR(back.x(), world.x(), 1e-3f);
		EXPECT_NEAR(back.y(), world.y(), 1e-3f);
		EXPECT_NEAR(back.z(), world.z(), 1e-3f);
	}
}

TEST(RendererIsometric, projectionFormulaMatchesRoadmap) {
	const IsometricConfig cfg;
	const math::vec3 world{3.f, 1.f, 2.f};
	const math::vec2 screen = worldToScreen(world, cfg);
	EXPECT_FLOAT_EQ(screen.x(), (3.f - 1.f) * 32.f);
	EXPECT_FLOAT_EQ(screen.y(), (3.f + 1.f) * 16.f - 2.f * cfg.zStep);
}

TEST(RendererIsometric, viewIsYawAndPitchOfAMirroredWorld) {
	const IsometricConfig cfg;
	const math::mat4 view = IsometricCamera::makeView(cfg, {0.f, 0.f, 0.f});
	const float pitch = renderer::getIsometricPitch(cfg);
	const float scale = static_cast<float>(cfg.tileSize.x()) / std::numbers::sqrt2_v<float>;
	const float heightScale = cfg.zStep / (scale * std::cos(pitch));
	// Undo the pixel scale, the Y mirror and the Z scale: what remains is a rotation.
	std::array<math::vec3, 3> rows{};
	for (size_t row = 0; row < 3; ++row)
		rows[row] = math::vec3{view(row, 0), -view(row, 1), view(row, 2) / heightScale} / scale;
	for (size_t i = 0; i < 3; ++i) {
		EXPECT_NEAR(rows[i].norm(), 1.f, 1e-5f);
		for (size_t j = i + 1; j < 3; ++j) EXPECT_NEAR(rows[i] * rows[j], 0.f, 1e-5f);
	}
	const float half = std::numbers::sqrt2_v<float> * 0.5f;
	EXPECT_NEAR(rows[0].x(), half, 1e-5f);
	EXPECT_NEAR(rows[0].y(), half, 1e-5f);
	EXPECT_NEAR(rows[2].z(), std::sin(pitch), 1e-5f);
}

TEST(RendererIsometric, cameraMatchesHelpersOnBothClipSpaces) {
	const IsometricCamera camera = makeCamera();
	const math::vec2ui viewport = camera.getViewportSize();
	for (const auto clipSpace: {CameraOrtho::ClipSpace::OpenGl, CameraOrtho::ClipSpace::Vulkan}) {
		for (const int32_t level: {-1, 0, 2}) {
			IsometricCamera zoomed = camera;
			zoomed.setZoomLevel(level);
			const math::mat4 viewProjection = IsometricCamera::makeProjection(viewport, zoomed.getZoom(), clipSpace) *
											  IsometricCamera::makeView(zoomed.getConfig(), zoomed.getTarget());
			for (const auto& world: {math::vec3{4.f, 3.f, 0.f}, math::vec3{6.f, 1.f, 0.f}, math::vec3{2.f, 5.f, 2.f}}) {
				const math::vec2 expected = zoomed.worldToViewport(world);
				const math::vec2 actual = projectToPixel(viewProjection, world, viewport, clipSpace);
				EXPECT_NEAR(actual.x(), expected.x(), 1e-2f);
				EXPECT_NEAR(actual.y(), expected.y(), 1e-2f);
			}
		}
	}
	const math::vec2 centre = camera.worldToViewport(camera.getTarget());
	EXPECT_FLOAT_EQ(centre.x(), 400.f);
	EXPECT_FLOAT_EQ(centre.y(), 300.f);
}

TEST(RendererIsometric, depthIsUsableAndNearerIsSmaller) {
	const IsometricCamera camera = makeCamera();
	const math::vec3 back{3.f, 2.f, 0.f};
	const math::vec3 front{5.f, 4.f, 0.f};
	const math::vec3 above{3.f, 2.f, 3.f};
	EXPECT_GT(camera.getViewDepth(front), camera.getViewDepth(back));
	EXPECT_GT(camera.getViewDepth(above), camera.getViewDepth(back));
	for (const auto clipSpace: {CameraOrtho::ClipSpace::OpenGl, CameraOrtho::ClipSpace::Vulkan}) {
		const math::mat4 viewProjection =
				IsometricCamera::makeProjection(camera.getViewportSize(), camera.getZoom(), clipSpace) *
				IsometricCamera::makeView(camera.getConfig(), camera.getTarget());
		const float minDepth = clipSpace == CameraOrtho::ClipSpace::Vulkan ? 0.f : -1.f;
		for (const auto& point: {back, front, above}) {
			const float depth = ndcDepth(viewProjection, point);
			EXPECT_GT(depth, minDepth);
			EXPECT_LT(depth, 1.f);
		}
		EXPECT_LT(ndcDepth(viewProjection, front), ndcDepth(viewProjection, back));
	}
}

TEST(RendererIsometric, viewportRoundTripPanAndZoom) {
	IsometricCamera camera = makeCamera();
	const math::vec3 world{6.f, 2.f, 1.f};
	const math::vec3 back = camera.viewportToWorld(camera.worldToViewport(world), world.z());
	EXPECT_NEAR(back.x(), world.x(), 1e-4f);
	EXPECT_NEAR(back.y(), world.y(), 1e-4f);

	const math::vec2 before = camera.worldToViewport(world);
	camera.pan({30.f, -12.f});
	const math::vec2 after = camera.worldToViewport(world);
	EXPECT_NEAR(after.x() - before.x(), 30.f, 1e-3f);
	EXPECT_NEAR(after.y() - before.y(), -12.f, 1e-3f);

	camera.setZoomLevel(1);
	EXPECT_FLOAT_EQ(camera.getZoom(), 2.f);
	camera.setZoomLevel(-100);
	EXPECT_EQ(camera.getZoomLevel(), int32_t{IsometricCamera::minZoomLevel});
	EXPECT_FLOAT_EQ(camera.getZoom(), 1.f / 16.f);
}

TEST(RendererIsometric, translucentOrderIsBackToFront) {
	const IsometricCamera camera = makeCamera();
	const std::vector<IsometricBillboard> billboards{
			{.foot = {5.f, 5.f, 0.f}, .color = {1.f, 1.f, 1.f, 0.5f}, .entityId = 0},
			{.foot = {1.f, 1.f, 0.f}, .color = {1.f, 1.f, 1.f, 0.5f}, .entityId = 1},
			{.foot = {3.f, 3.f, 0.f}, .color = {1.f, 1.f, 1.f, 0.5f}, .depthBias = 1.f, .entityId = 2},
			{.foot = {3.f, 3.f, 0.f}, .color = {1.f, 1.f, 1.f, 0.5f}, .entityId = 3},
			{.foot = {4.f, 3.f, 0.f}, .color = {1.f, 1.f, 1.f, 0.5f}, .entityId = 4},
	};
	const auto order = getBackToFrontOrder(camera, billboards);
	EXPECT_EQ(order, (std::vector<uint32_t>{1, 3, 2, 4, 0}));
}

TEST(RendererIsometric, pickedCellIsTheOneUnderTheCursor) {
	IsometricCamera camera = makeCamera();
	camera.setZoomLevel(1);
	const math::vec3 origin{-2.f, 1.f, 0.5f};
	const math::vec2ui size{8u, 6u};
	for (const auto& cell: {math::vec2ui{0u, 0u}, math::vec2ui{3u, 2u}, math::vec2ui{7u, 5u}}) {
		const math::vec3 centre =
				origin + math::vec3{static_cast<float>(cell.x()) + 0.5f, static_cast<float>(cell.y()) + 0.5f, 0.f};
		const auto picked = pickIsometricCell(camera, camera.worldToViewport(centre), origin, size);
		ASSERT_TRUE(picked.has_value());
		EXPECT_EQ(picked->x(), cell.x());
		EXPECT_EQ(picked->y(), cell.y());
	}
	// Just outside the front corner of the map.
	const math::vec3 outside = origin + math::vec3{8.2f, 6.2f, 0.f};
	EXPECT_FALSE(pickIsometricCell(camera, camera.worldToViewport(outside), origin, size).has_value());
}

TEST(RendererIsometric, drawCountsOpaqueAndTranslucentBillboards) {
	bootRendererStack();
	const IsometricCamera camera = makeCamera();
	RendererIsometric::resetStats();
	RendererIsometric::beginScene(camera);
	RendererIsometric::drawBillboard({.foot = {4.f, 3.f, 0.f}, .entityId = 1});
	RendererIsometric::drawBillboard({.foot = {5.f, 3.f, 0.f}, .color = {1.f, 1.f, 1.f, 0.4f}, .entityId = 2});
	const auto map = makeIsometricMap();
	RendererIsometric::drawTilemap(map, math::Transform{}, 3);
	RendererIsometric::endScene();
	const auto stats = RendererIsometric::getStatistics();
	EXPECT_EQ(stats.opaqueCount, 1u + 64u + 1u);
	EXPECT_EQ(stats.translucentCount, 1u);
	EXPECT_EQ(stats.drawCallCount, 4u) << "one per tilemap chunk (two layers), the opaque batch, the translucent batch";
	teardownRendererStack();
}

TEST(RendererIsometric, tilemapDrawsOverlaysAndFoundationsOnSlopes) {
	bootRendererStack();
	const IsometricCamera camera = makeCamera();
	auto map = makeIsometricMap();
	std::ignore = map.setCornerHeight(3, 3, 2);
	scene::SlopeSet grass;
	grass.id = 0;
	grass.foundations.fill(1);
	map.tileset->slopeSets.push_back(grass);
	map.layers[1].setCell(2, 2, {.terrain = scene::component::g_EmptyTileIndex, .overlay = 2, .flags = 0});
	map.layers[1].setCell(5, 5, {.terrain = scene::component::g_EmptyTileIndex, .overlay = 2, .flags = 0});
	map.layers[0].setCell(
			3, 3,
			{.terrain = 0, .overlay = scene::component::g_EmptyTileIndex, .flags = scene::terrain::foundationFlag});
	RendererIsometric::resetStats();
	RendererIsometric::beginScene(camera);
	RendererIsometric::drawTilemap(map, math::Transform{}, 3);
	RendererIsometric::endScene();
	EXPECT_EQ(RendererIsometric::getStatistics().opaqueCount, 64u + 1u + 2u) << "terrain, a road tile and two overlays";
	teardownRendererStack();
}

TEST(RendererIsometric, tilemapChunksOutsideTheViewAreCulled) {
	bootRendererStack();
	IsometricCamera camera = makeCamera();
	camera.setTarget({500.f, -500.f, 0.f});
	RendererIsometric::resetStats();
	RendererIsometric::beginScene(camera);
	const auto map = makeIsometricMap();
	RendererIsometric::drawTilemap(map, math::Transform{}, 3);
	RendererIsometric::endScene();
	const auto stats = RendererIsometric::getStatistics();
	EXPECT_EQ(stats.opaqueCount, 0u);
	EXPECT_EQ(stats.culledChunkCount, 2u);
	EXPECT_EQ(stats.drawCallCount, 0u);
	teardownRendererStack();
}

TEST(RendererIsometric, layerUsesAnIsometricCameraAsIs) {
	RendererIsometricLayer layer{"iso"};
	layer.applyConfig("{Zoom: 1}");
	layer.setViewport({640u, 480u});
	IsometricCamera editor = makeCamera();
	editor.setZoomLevel(-2);
	const auto fromEditor = layer.makeCamera(editor);
	EXPECT_EQ(fromEditor.getZoomLevel(), -2);
	EXPECT_EQ(fromEditor.getViewportSize().x(), 640u);

	CameraOrtho sceneCamera{-1.f, 1.f, -1.f, 1.f};
	sceneCamera.setPosition({7.f, 2.f, 5.f});
	const auto fromScene = layer.makeCamera(sceneCamera);
	EXPECT_EQ(fromScene.getZoomLevel(), 1);
	EXPECT_NEAR(fromScene.getTarget().x(), 7.f, 1e-5f);
	EXPECT_NEAR(fromScene.getTarget().y(), 2.f, 1e-5f);
	EXPECT_NEAR(fromScene.getTarget().z(), 0.f, 1e-5f);
}

TEST(RendererIsometric, packColorWritesRgbaBytesRedLowest) {
	EXPECT_EQ(packColor({1.f, 1.f, 1.f, 1.f}), 0xFFFFFFFFu);
	EXPECT_EQ(packColor({1.f, 0.f, 0.f, 0.f}), 0x000000FFu);
	EXPECT_EQ(packColor({0.f, 0.f, 0.f, 1.f}), 0xFF000000u);
	EXPECT_EQ(packColor({0.f, 1.f, 0.f, 0.f}), 0x0000FF00u);
	EXPECT_EQ(packColor({2.f, -1.f, 0.5f, 1.f}), 0xFF8000FFu) << "channels are clamped to [0, 1]";
}

TEST(RendererIsometric, spriteScreenRectFollowsTheAnchor) {
	IsometricCamera camera = makeCamera();
	camera.setTarget({2.f, 2.f, 0.f});
	const math::vec2 foot = camera.worldToViewport({2.f, 2.f, 0.f});
	const math::vec4 rect = getSpriteScreenRect(camera, {.foot = {2.f, 2.f, 0.f}, .size = {40.f, 30.f}});
	EXPECT_FLOAT_EQ(rect.x(), foot.x() - 20.f);
	EXPECT_FLOAT_EQ(rect.z(), foot.x() + 20.f);
	EXPECT_FLOAT_EQ(rect.w(), foot.y());
	EXPECT_FLOAT_EQ(rect.y(), foot.y() - 30.f);
	const math::vec4 centred =
			getSpriteScreenRect(camera, {.foot = {2.f, 2.f, 0.f}, .size = {40.f, 30.f}, .anchor = {0.5f, 0.5f}});
	EXPECT_FLOAT_EQ(centred.w(), foot.y() + 15.f);
	camera.setZoomLevel(1);
	const math::vec4 doubled = getSpriteScreenRect(camera, {.foot = {2.f, 2.f, 0.f}, .size = {40.f, 30.f}});
	EXPECT_FLOAT_EQ(doubled.z() - doubled.x(), 80.f) << "the rectangle grows with the zoom";
}

TEST(RendererIsometric, spritesOutsideTheViewAreDropped) {
	bootRendererStack();
	const IsometricCamera camera = makeCamera();
	const auto texture = renderer::gpu::Texture2D::create(renderer::gpu::Texture2D::Specification{.size = {8, 8}});
	std::vector<IsometricSprite> sprites;
	for (int i = 0; i < 10; ++i) sprites.push_back({.foot = {static_cast<float>(i) * 0.1f, 0.f, 0.f}, .entityId = i});
	sprites.push_back({.foot = {900.f, -900.f, 0.f}});
	sprites.push_back({.foot = {-900.f, 900.f, 0.f}});
	RendererIsometric::resetStats();
	RendererIsometric::beginScene(camera);
	RendererIsometric::drawSprites(texture, sprites);
	RendererIsometric::endScene();
	const auto stats = RendererIsometric::getStatistics();
	EXPECT_EQ(stats.spriteCount, 10u);
	EXPECT_EQ(stats.culledSpriteCount, 2u);
	EXPECT_EQ(stats.drawCallCount, 1u);
	teardownRendererStack();
}

TEST(RendererIsometric, thousandsOfSpritesShareOneDrawCall) {
	bootRendererStack();
	IsometricCamera camera = makeCamera();
	camera.setZoomLevel(-2);
	const auto texture = renderer::gpu::Texture2D::create(renderer::gpu::Texture2D::Specification{.size = {8, 8}});
	std::vector<IsometricSprite> sprites;
	for (uint32_t i = 0; i < 5000; ++i) {
		const uint32_t row = i / 100;
		sprites.push_back({.foot = {static_cast<float>(i % 100) * 0.4f, static_cast<float>(row) * 0.4f, 0.f},
						   .size = {16.f, 8.f},
						   .color = packColor({0.5f, 0.5f, 0.5f, 1.f})});
	}
	RendererIsometric::resetStats();
	RendererIsometric::beginScene(camera);
	RendererIsometric::drawSprites(texture, sprites);
	RendererIsometric::endScene();
	const auto stats = RendererIsometric::getStatistics();
	EXPECT_EQ(stats.spriteCount + stats.culledSpriteCount, 5000u);
	EXPECT_GT(stats.spriteCount, 4000u);
	EXPECT_EQ(stats.drawCallCount, 1u);
	teardownRendererStack();
}

TEST(RendererIsometric, objectDrawsOneSpritePerSlice) {
	bootRendererStack();
	const IsometricCamera camera = makeCamera();
	auto parsed = IsometricObjectSet::parse(
			"{zoom: 1x, buildings: {wide: {footprint: [3, 1], slices: ["
			"{cell: [0, 0], rect: [0, 0, 64, 128], anchor: [32, 128], box: {min: [0, 0, 0], max: [1, 1, 2]}},"
			"{cell: [1, 0], rect: [64, 0, 64, 128], anchor: [32, 128], box: {min: [1, 0, 0], max: [2, 1, 2]}},"
			"{cell: [2, 0], rect: [128, 0, 64, 128], anchor: [32, 128], box: {min: [2, 0, 0], max: [3, 1, 2]}}]}}}");
	ASSERT_TRUE(parsed.has_value());
	IsometricObjectSet set = std::move(*parsed);
	RendererIsometric::resetStats();
	RendererIsometric::beginScene(camera);
	RendererIsometric::drawObject(set, *set.find("wide"), {3.f, 3.f, 0.f}, {1.f, 1.f, 1.f, 1.f}, 7);
	RendererIsometric::endScene();
	EXPECT_EQ(RendererIsometric::getStatistics().spriteCount, 0u) << "a set without texture draws nothing";
	set.setTexture(renderer::gpu::Texture2D::create(renderer::gpu::Texture2D::Specification{.size = {256, 128}}));
	RendererIsometric::resetStats();
	RendererIsometric::beginScene(camera);
	RendererIsometric::drawObject(set, *set.find("wide"), {3.f, 3.f, 0.f}, {1.f, 1.f, 1.f, 1.f}, 7);
	RendererIsometric::endScene();
	const auto stats = RendererIsometric::getStatistics();
	EXPECT_EQ(stats.spriteCount, 3u);
	EXPECT_EQ(stats.drawCallCount, 1u);
	teardownRendererStack();
}

namespace {

constexpr uint64_t g_tileInstanceBytes = 32;

auto makeBigMap() -> scene::TilemapAsset {
	auto map = makeIsometricMap();
	map.resize(64, 64);
	map.layers[0].fill(0);
	return map;
}

auto prepareOnce(const scene::TilemapAsset& iMap) -> RendererIsometric::PrepareStatistics {
	RendererIsometric::beginPrepare();
	RendererIsometric::prepareTilemap(iMap);
	return RendererIsometric::getPrepareStatistics();
}

}// namespace

TEST(RendererIsometric, firstPrepareUploadsEveryChunkOnce) {
	bootRendererStack();
	const auto map = makeBigMap();
	const auto first = prepareOnce(map);
	EXPECT_EQ(first.builtChunkCount, 8u) << "four chunks for each of the two layers";
	EXPECT_EQ(first.lateBuiltChunkCount, 0u);
	EXPECT_GE(first.uploadedBytes, uint64_t{64} * 64 * g_tileInstanceBytes);
	EXPECT_EQ(RendererIsometric::getCachedAssetCount(), 1u);
	teardownRendererStack();
}

TEST(RendererIsometric, staticFramesUploadNothing) {
	bootRendererStack();
	const auto map = makeBigMap();
	std::ignore = prepareOnce(map);
	IsometricCamera camera = makeCamera();
	camera.setTarget({32.f, 32.f, 0.f});
	for (int frame = 0; frame < 3; ++frame) {
		const auto stats = prepareOnce(map);
		EXPECT_EQ(stats.builtChunkCount, 0u);
		EXPECT_EQ(stats.uploadedBytes, 0u);
		RendererIsometric::resetStats();
		RendererIsometric::beginScene(camera);
		RendererIsometric::drawTilemap(map, math::Transform{}, 3);
		RendererIsometric::endScene();
		EXPECT_GT(RendererIsometric::getStatistics().opaqueCount, 0u);
		const auto drawn = RendererIsometric::getPrepareStatistics();
		EXPECT_EQ(drawn.lateBuiltChunkCount, 0u) << "a prepared map is never built at draw time";
		EXPECT_EQ(drawn.uploadedBytes, 0u);
	}
	teardownRendererStack();
}

TEST(RendererIsometric, movingTheCameraOrTheMapUploadsNothing) {
	bootRendererStack();
	const auto map = makeBigMap();
	std::ignore = prepareOnce(map);
	RendererIsometric::beginPrepare();
	IsometricCamera camera = makeCamera();
	for (const float shift: {0.f, 10.f, 40.f}) {
		camera.setTarget({shift, shift, 0.f});
		camera.setZoomLevel(shift > 20.f ? -1 : 0);
		RendererIsometric::beginScene(camera);
		math::Transform transform;
		transform.translation() = {shift, -shift, 2.f};
		RendererIsometric::drawTilemap(map, transform, 3);
		RendererIsometric::endScene();
	}
	const auto stats = RendererIsometric::getPrepareStatistics();
	EXPECT_EQ(stats.builtChunkCount, 0u);
	EXPECT_EQ(stats.uploadedBytes, 0u);
	teardownRendererStack();
}

TEST(RendererIsometric, oneCellEditUploadsOneChunk) {
	bootRendererStack();
	auto map = makeBigMap();
	std::ignore = prepareOnce(map);
	map.setTile(0, 40, 5, 2);
	const auto stats = prepareOnce(map);
	EXPECT_EQ(stats.builtChunkCount, 1u);
	EXPECT_EQ(stats.uploadedBytes, uint64_t{32} * 32 * g_tileInstanceBytes);
	EXPECT_EQ(prepareOnce(map).uploadedBytes, 0u);
	teardownRendererStack();
}

TEST(RendererIsometric, overlayEditAddsAnInstanceInItsChunkOnly) {
	bootRendererStack();
	auto map = makeBigMap();
	std::ignore = prepareOnce(map);
	map.layers[1].setCell(3, 3, {.terrain = scene::component::g_EmptyTileIndex, .overlay = 2, .flags = 0});
	const auto stats = prepareOnce(map);
	EXPECT_EQ(stats.builtChunkCount, 1u);
	EXPECT_EQ(stats.uploadedBytes, uint64_t{2} * g_tileInstanceBytes) << "the road tile and the overlay";
	teardownRendererStack();
}

TEST(RendererIsometric, cornerHeightChangeRebuildsTheChunksAroundTheCorner) {
	bootRendererStack();
	auto map = makeBigMap();
	std::ignore = prepareOnce(map);
	std::ignore = map.setCornerHeight(5, 5, 1);
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 2u) << "an inner corner touches one chunk of each of the two layers";
	const auto changes = map.setCornerHeight(32, 5, 1);
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 4u) << "a corner on a chunk border touches two chunks per layer";
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 0u);
	map.applyCornerChanges(changes, true);
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 4u) << "an undo is a change like any other";
	teardownRendererStack();
}

TEST(RendererIsometric, tilesetChangeRebuildsEveryChunk) {
	bootRendererStack();
	auto map = makeBigMap();
	std::ignore = prepareOnce(map);
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 0u);
	scene::SlopeSet grass;
	grass.id = 0;
	grass.foundations.fill(1);
	map.tileset->slopeSets.push_back(grass);
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 8u) << "a slope table changes the tile of the cells";
	map.tileset->columns = 4;
	map.tileset->tiles.resize(map.tileset->tileCount());
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 8u) << "the atlas layout changes the texture rectangles";
	map.tileset = mkShared<scene::Tileset>(*map.tileset);
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 8u) << "another tileset object";
	EXPECT_EQ(prepareOnce(map).builtChunkCount, 0u);
	teardownRendererStack();
}

TEST(RendererIsometric, unpreparedMapIsBuiltAtDrawTimeOnce) {
	bootRendererStack();
	const auto map = makeBigMap();
	const IsometricCamera camera = makeCamera();
	for (int frame = 0; frame < 2; ++frame) {
		RendererIsometric::beginPrepare();
		RendererIsometric::beginScene(camera);
		RendererIsometric::drawTilemap(map, math::Transform{}, 3);
		RendererIsometric::endScene();
		const auto stats = RendererIsometric::getPrepareStatistics();
		EXPECT_EQ(stats.lateBuiltChunkCount, frame == 0 ? 8u : 0u);
		EXPECT_EQ(stats.builtChunkCount, frame == 0 ? 8u : 0u);
	}
	teardownRendererStack();
}

TEST(RendererIsometric, unusedMapIsEvictedFromTheCache) {
	bootRendererStack();
	{
		const auto map = makeBigMap();
		std::ignore = prepareOnce(map);
		EXPECT_EQ(RendererIsometric::getCachedAssetCount(), 1u);
	}
	for (uint64_t i = 0; i <= RendererIsometric::cacheEvictionPrepares; ++i) RendererIsometric::beginPrepare();
	EXPECT_EQ(RendererIsometric::getCachedAssetCount(), 0u);
	teardownRendererStack();
}
