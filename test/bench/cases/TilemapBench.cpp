/**
 * @file TilemapBench.cpp
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <renderer/CameraOrtho.h>
#include <renderer/Renderer2D.h>
#include <renderer/RendererTilemap.h>
#include <renderer/gpu/Texture.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <tuple>

namespace owl::bench {

namespace {

using renderer::RendererTilemap;

auto makeTileset() -> shared<scene::Tileset> {
	auto tileset = mkShared<scene::Tileset>();
	tileset->columns = 8;
	tileset->rows = 8;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(tileset->tileCount());
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {128, 128}, .format = renderer::gpu::ImageFormat::Rgba8});
	return tileset;
}

// Every cell of every layer holds a tile: the worst case for chunk builds and draws.
auto makeMap(const uint32_t iSize, const uint32_t iLayers) -> scene::TilemapAsset {
	scene::TilemapAsset map;
	map.width = iSize;
	map.height = iSize;
	map.tileset = makeTileset();
	for (uint32_t l = 0; l < iLayers; ++l) {
		auto& layer = map.addLayer(std::format("layer{}", l));
		for (uint32_t y = 0; y < iSize; ++y)
			for (uint32_t x = 0; x < iSize; ++x) layer.setTile(x, y, static_cast<int32_t>((x + y + l) % 64));
	}
	return map;
}

void renderFrame(const scene::TilemapAsset& iMap, const renderer::CameraOrtho& iCamera) {
	RendererTilemap::beginPrepare();
	RendererTilemap::prepareTilemap(iMap);
	renderer::Renderer2D::beginScene(iCamera);
	RendererTilemap::drawTilemap(iMap, math::Transform{}, /*iEntityId=*/0);
	renderer::Renderer2D::endScene();
}

void runStaticFrames(Runner& ioRunner, const renderer::CameraOrtho& iCamera) {
	for (const uint32_t size: {32U, 128U, 256U, 1024U}) {
		for (const uint32_t layers: {1U, 4U}) {
			const auto name = std::format("tilemap/static/{}x{}_layers{}", size, size, layers);
			if (!ioRunner.wants(name))
				continue;
			const auto map = makeMap(size, layers);
			renderFrame(map, iCamera);
			ioRunner.metric(name + "_first_upload_bytes",
							static_cast<double>(RendererTilemap::getPrepareStatistics().uploadedBytes), "bytes");
			ioRunner.measure(name, static_cast<uint64_t>(size) * size * layers,
							 [&]() -> void { renderFrame(map, iCamera); });
			renderFrame(map, iCamera);
			ioRunner.metric(name + "_uploaded_bytes",
							static_cast<double>(RendererTilemap::getPrepareStatistics().uploadedBytes),
							"bytes (one static frame)");
			ioRunner.metric(name + "_draw_calls", RendererTilemap::getStatistics().drawCallCount,
							"draw calls (one frame)");
			RendererTilemap::clearCache();
		}
	}
}

void runEdits(Runner& ioRunner, const renderer::CameraOrtho& iCamera) {
	if (ioRunner.wants("tilemap/edit_one_cell")) {
		auto map = makeMap(256, 4);
		renderFrame(map, iCamera);
		int32_t value = 0;
		ioRunner.measure("tilemap/edit_one_cell/256x256_layers4", 1, [&]() -> void {
			value = (value + 1) % 64;
			map.setTile(1, 100, 100, value);
			renderFrame(map, iCamera);
		});
		map.setTile(1, 100, 100, (map.getTile(1, 100, 100) + 1) % 64);
		renderFrame(map, iCamera);
		const auto stats = RendererTilemap::getPrepareStatistics();
		ioRunner.metric("tilemap/edit_one_cell/256x256_layers4_chunks", stats.builtChunkCount, "chunks uploaded");
		ioRunner.metric("tilemap/edit_one_cell/256x256_layers4_bytes", static_cast<double>(stats.uploadedBytes),
						"bytes uploaded");
		RendererTilemap::clearCache();
	}
	if (ioRunner.wants("tilemap/build_chunk")) {
		auto map = makeMap(32, 1);
		RendererTilemap::beginPrepare();
		RendererTilemap::prepareTilemap(map);
		int32_t value = 0;
		ioRunner.measure("tilemap/build_chunk/32x32", 32 * 32, [&]() -> void {
			value = (value + 1) % 64;
			map.setTile(0, 0, 0, value);
			RendererTilemap::prepareTilemap(map);
		});
		RendererTilemap::clearCache();
	}
}

void runLoad(Runner& ioRunner) {
	if (!ioRunner.wants("tilemap/load"))
		return;
	const auto map = makeMap(1024, 1);
	const auto path = std::filesystem::temp_directory_path() / "owl_bench_tilemap_1024.owltilemap";
	if (!map.saveToFile(path))
		return;
	ioRunner.measure("tilemap/load/1024x1024", 1024ULL * 1024ULL, [&]() -> void {
		scene::TilemapAsset loaded;
		std::ignore = loaded.loadFromFile(path);
	});
	ioRunner.metric("tilemap/load/1024x1024_tile_bytes", static_cast<double>(map.getStorageBytes()),
					"bytes of cells in memory");
	ioRunner.metric("tilemap/load/1024x1024_file_bytes", static_cast<double>(std::filesystem::file_size(path)),
					"bytes on disk");
	std::filesystem::remove(path);
}

}// namespace

void runTilemapBenches(Runner& ioRunner) {
	if (!ioRunner.wants("tilemap"))
		return;
	// About 1080p at 30 pixels per cell, centred on the map.
	const renderer::CameraOrtho camera(-32, 32, -18, 18);
	runStaticFrames(ioRunner, camera);
	runEdits(ioRunner, camera);
	runLoad(ioRunner);
}

}// namespace owl::bench
