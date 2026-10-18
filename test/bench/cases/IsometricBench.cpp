/**
 * @file IsometricBench.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <math/Transform.h>
#include <math/vectors.h>
#include <renderer/IsometricObjects.h>
#include <renderer/RendererIsometric.h>
#include <renderer/gpu/Texture.h>
#include <scene/TilemapAsset.h>
#include <scene/TilemapTerrain.h>
#include <scene/Tileset.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::bench {

namespace {

using renderer::IsometricSprite;
using renderer::RendererIsometric;

auto makeTileset() -> shared<scene::Tileset> {
	auto tileset = mkShared<scene::Tileset>();
	tileset->kind = scene::Tileset::Kind::Isometric;
	tileset->columns = 4;
	tileset->rows = 4;
	tileset->tileWidth = 64;
	tileset->tileHeight = 48;
	tileset->tiles.resize(tileset->tileCount());
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {256, 192}, .format = renderer::gpu::ImageFormat::Rgba8});
	return tileset;
}

// Rolling hills: a raised corner every 16 cells, so the corner heights reach 8 steps and slopes fill the map.
auto makeHills(const uint32_t iSize) -> scene::TilemapAsset {
	scene::TilemapAsset map;
	map.width = iSize;
	map.height = iSize;
	map.tileset = makeTileset();
	auto& ground = map.addLayer("ground");
	for (uint32_t y = 0; y < iSize; ++y)
		for (uint32_t x = 0; x < iSize; ++x) ground.setTile(x, y, static_cast<int32_t>((x + y) % 4));
	for (uint32_t y = 8; y < iSize; y += 16) {
		for (uint32_t x = 8; x < iSize; x += 16) std::ignore = map.setCornerHeight(x, y, 8);
	}
	return map;
}

auto makeCamera(const int32_t iZoomLevel, const float iCentre) -> renderer::IsometricCamera {
	renderer::IsometricCamera camera;
	camera.setViewportSize({1920U, 1080U});
	camera.setZoomLevel(iZoomLevel);
	camera.setTarget({iCentre, iCentre, 0.f});
	return camera;
}

// Vehicles on a regular lattice around a centre, all inside the viewport when the spacing is small enough.
auto makeVehicles(const uint32_t iCount, const float iCentre, const float iSpacing) -> std::vector<IsometricSprite> {
	std::vector<IsometricSprite> vehicles;
	vehicles.reserve(iCount);
	const auto side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(iCount))));
	const float origin = iCentre - static_cast<float>(side) * iSpacing * 0.5f;
	for (uint32_t i = 0; i < iCount; ++i) {
		vehicles.push_back({.foot = {origin + static_cast<float>(i % side) * iSpacing,
									 origin + static_cast<float>(i / side) * iSpacing, 0.f},
							.size = {64.f, 64.f},
							.color = renderer::packColor({0.9f, 0.9f, 0.9f, 1.f}),
							.entityId = static_cast<int>(i)});
	}
	return vehicles;
}

void runFrame(Runner& ioRunner, const std::string& iName, const scene::TilemapAsset& iMap,
			  const renderer::IsometricCamera& iCamera, const shared<renderer::gpu::Texture2D>& iTexture,
			  const std::vector<IsometricSprite>& iVehicles) {
	const auto frame = [&]() -> void {
		RendererIsometric::beginScene(iCamera);
		RendererIsometric::drawTilemap(iMap, math::Transform{}, 0);
		RendererIsometric::drawSprites(iTexture, iVehicles);
		RendererIsometric::endScene();
	};
	ioRunner.measure(iName, 1, frame);
	if (!ioRunner.wants(iName))
		return;
	RendererIsometric::resetStats();
	frame();
	const auto stats = RendererIsometric::getStatistics();
	ioRunner.metric(iName + "_cells", stats.opaqueCount, "tiles drawn (one frame)");
	ioRunner.metric(iName + "_sprites", stats.spriteCount, "sprites drawn (one frame)");
	ioRunner.metric(iName + "_sprites_culled", stats.culledSpriteCount, "sprites outside the view (one frame)");
	ioRunner.metric(iName + "_draw_calls", stats.drawCallCount, "draw calls (one frame)");
}

void runFrames(Runner& ioRunner) {
	if (!ioRunner.wants("iso/frame"))
		return;
	constexpr uint32_t size = 512;
	constexpr float centre = static_cast<float>(size) * 0.5f;
	const auto map = makeHills(size);
	const auto texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});
	const std::vector<IsometricSprite> none;
	runFrame(ioRunner, "iso/frame/512x512_1080p_zoom1", map, makeCamera(0, centre), texture, none);
	runFrame(ioRunner, "iso/frame/512x512_1080p_zoom1_5000_vehicles_spread", map, makeCamera(0, centre), texture,
			 makeVehicles(5000, centre, 3.f));
	runFrame(ioRunner, "iso/frame/512x512_1080p_zoom1_5000_vehicles_in_view", map, makeCamera(0, centre), texture,
			 makeVehicles(5000, centre, 0.1f));
	runFrame(ioRunner, "iso/frame/512x512_1080p_zoom_out4", map, makeCamera(-2, centre), texture, none);
}

void runObjects(Runner& ioRunner) {
	if (!ioRunner.wants("iso/objects"))
		return;
	auto parsed = renderer::IsometricObjectSet::parse(
			"{zoom: 1x, buildings: {wide: {footprint: [3, 1], slices: ["
			"{cell: [0, 0], rect: [0, 0, 64, 128], anchor: [32, 128], box: {min: [0, 0, 0], max: [1, 1, 2]}},"
			"{cell: [1, 0], rect: [64, 0, 64, 128], anchor: [32, 128], box: {min: [1, 0, 0], max: [2, 1, 2]}},"
			"{cell: [2, 0], rect: [128, 0, 64, 128], anchor: [32, 128], box: {min: [2, 0, 0], max: [3, 1, 2]}}]}}}");
	if (!parsed)
		return;
	renderer::IsometricObjectSet set = std::move(*parsed);
	set.setTexture(renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {256, 128}, .format = renderer::gpu::ImageFormat::Rgba8}));
	const auto camera = makeCamera(0, 0.f);
	static constexpr uint32_t count = 1000;
	const auto* object = set.find("wide");
	const auto frame = [&]() -> void {
		RendererIsometric::beginScene(camera);
		for (uint32_t i = 0; i < count; ++i) {
			const float x = static_cast<float>(i % 40) * 1.5f - 20.f;
			const float y = static_cast<float>(i / 40) * 1.5f - 20.f;
			RendererIsometric::drawObject(set, *object, {x, y, 0.f}, {1.f, 1.f, 1.f, 1.f}, static_cast<int>(i));
		}
		RendererIsometric::endScene();
	};
	ioRunner.measure("iso/objects/1000_buildings_3_slices", count, frame);
	if (!ioRunner.wants("iso/objects/1000_buildings_3_slices"))
		return;
	RendererIsometric::resetStats();
	frame();
	const auto stats = RendererIsometric::getStatistics();
	ioRunner.metric("iso/objects/1000_buildings_3_slices_sprites", stats.spriteCount, "slices drawn (one frame)");
	ioRunner.metric("iso/objects/1000_buildings_3_slices_draw_calls", stats.drawCallCount, "draw calls (one frame)");
}

void runPicking(Runner& ioRunner) {
	constexpr uint32_t size = 256;
	const auto map = makeHills(size);
	renderer::IsometricCamera camera;
	camera.setViewportSize({1920U, 1080U});
	camera.setTarget({static_cast<float>(size) * 0.5f, static_cast<float>(size) * 0.5f, 0.f});
	const math::vec3 origin{0.f, 0.f, 0.f};
	std::array<math::vec2, 16> pixels{};
	for (size_t i = 0; i < pixels.size(); ++i)
		pixels[i] = {120.f + static_cast<float>(i % 4) * 560.f, 90.f + static_cast<float>(i / 4) * 280.f};
	size_t cursor = 0;
	uint32_t hits = 0;
	ioRunner.measure("iso/pick/256x256_hills", 1, [&]() -> void {
		cursor = (cursor + 1) % pixels.size();
		hits += scene::terrain::pickTerrain(map, camera, pixels[cursor], origin).has_value() ? 1U : 0U;
	});
	ioRunner.metric("iso/pick/256x256_hills_hits", hits, "picks that found the terrain (budget: 20 us each)");
}

}// namespace

void runIsometricBenches(Runner& ioRunner) {
	if (!ioRunner.wants("iso"))
		return;
	runPicking(ioRunner);
	runFrames(ioRunner);
	runObjects(ioRunner);
}

}// namespace owl::bench
