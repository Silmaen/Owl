/**
 * @file RendererTilemap.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/RendererTilemap.h"

#include "renderer/Renderer.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"
#include "renderer/utils/Lighting2DState.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace owl::renderer {

namespace {
using scene::component::TilemapLayer;

constexpr uint32_t g_chunkCells = TilemapLayer::chunkSize * TilemapLayer::chunkSize;
constexpr uint32_t g_minChunkCapacity = 64;
constexpr size_t g_maxTextureSlots = 32;
constexpr float g_layerDepthStep = 1e-4f;
constexpr uint32_t g_cellCoordShift = 16u;

// Per-draw uniform block, mirrored by `TilemapDraw` in `tilemap_instanced.slang`.
struct DrawUbo {
	math::mat4 viewProjection;
	math::mat4 model;
	// Grid origin (local centre of cell (0, 0)) x, y; cell size; layer depth.
	math::vec4 grid;
	// Half texel x, y; parallax offset x, y.
	math::vec4 texel;
	// Atlas columns, rows; entity id; texture slot.
	std::array<int32_t, 4> atlas{};
	// x: 1 when the bound framebuffer stores linear colour; y: 2D light count; zw: padding to a std140 vec4.
	std::array<int32_t, 4> flags{};
	// rgb: linear ambient of the 2D lights; w: 1 when the 2D lighting is on.
	math::vec4 ambient{1.f, 1.f, 1.f, 0.f};
};

// One non-empty cell of a chunk: layer cell coordinates packed as `x | y << 16`, then the tile index.
struct CellInstance {
	uint32_t cell = 0;
	int32_t tileIndex = -1;
};

struct ChunkGpu {
	uint64_t generation = 0;
	uint32_t instanceCount = 0;
	uint32_t capacity = 0;
	shared<gpu::DrawData> drawData;
};

struct LayerGpu {
	uint64_t generation = 0;
	math::vec2ui size{0, 0};
	math::vec2ui chunkGrid{0, 0};
	uint32_t occupiedChunkCount = 0;
	std::vector<ChunkGpu> chunks;
};

struct AssetGpu {
	std::vector<LayerGpu> layers;
	uint64_t lastUsedPrepare = 0;
};

struct PendingTilemap {
	const scene::TilemapAsset* asset = nullptr;
	math::Transform transform;
	int entityId = -1;
};

struct InternalState {
	bool initialized = false;
	shared<gpu::UniformBuffer> drawUbo;
	DrawUbo drawBuffer;
	math::mat4 viewProjection = math::identity<float, 4>();
	math::vec2 cameraPosition{0.f, 0.f};
	std::vector<uint32_t> quadIndices{0, 1, 2, 2, 3, 0};
	std::vector<CellInstance> instanceScratch;
	std::vector<shared<gpu::Texture2D>> textureScratch;
	std::vector<const scene::Tileset*> tilesetSlots;
	std::vector<PendingTilemap> pending;
	std::unordered_map<const scene::TilemapAsset*, AssetGpu> cache;
	uint64_t prepareIndex = 0;
	RendererTilemap::Statistics stats;
	RendererTilemap::PrepareStatistics prepareStats;
};

shared<InternalState> g_state;

constexpr auto g_rendererKey = "tilemap_instanced";

// Identifier of this renderer's descriptor block, resolved once.
auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

auto isReady() -> bool { return g_state && g_state->initialized; }

auto makeChunkDrawData(const uint32_t iCapacity) -> shared<gpu::DrawData> {
	const gpu::BufferLayout vertexLayout{
			{"i_CornerIndex", gpu::ShaderDataType::Int},
	};
	const gpu::BufferLayout instanceLayout{
			{"i_Cell", gpu::ShaderDataType::Int},
			{"i_TileIndex", gpu::ShaderDataType::Int},
	};
	auto drawData = gpu::DrawData::create();
	drawData->initInstanced(vertexLayout, instanceLayout, /*iVertexCapacity=*/4u, iCapacity, g_rendererKey,
							g_state->quadIndices, g_rendererKey, {});
	constexpr std::array<int32_t, 4> cornerIndices{0, 1, 2, 3};
	drawData->setVertexData(cornerIndices.data(), static_cast<uint32_t>(cornerIndices.size() * sizeof(int32_t)));
	g_state->prepareStats.uploadedBytes += cornerIndices.size() * sizeof(int32_t);
	return drawData;
}

void collectChunkCells(const TilemapLayer& iLayer, const math::vec2ui& iChunk, std::vector<CellInstance>& oCells) {
	oCells.clear();
	const auto* chunk = iLayer.getChunk(iChunk);
	if (chunk == nullptr)
		return;
	const auto& size = iLayer.getSize();
	const uint32_t x0 = iChunk.x() * TilemapLayer::chunkSize;
	const uint32_t y0 = iChunk.y() * TilemapLayer::chunkSize;
	const uint32_t x1 = std::min(size.x(), x0 + TilemapLayer::chunkSize);
	const uint32_t y1 = std::min(size.y(), y0 + TilemapLayer::chunkSize);
	// Every cell is written and only the non-empty ones are kept: no branch on the tile in the loop.
	oCells.resize(g_chunkCells);
	size_t count = 0;
	for (uint32_t y = y0; y < y1; ++y) {
		const uint32_t row = (y - y0) * TilemapLayer::chunkSize;
		for (uint32_t x = x0; x < x1; ++x) {
			const uint16_t id = chunk->terrain[row + x - x0];
			oCells[count] = {.cell = x | (y << g_cellCoordShift), .tileIndex = static_cast<int32_t>(id)};
			count += id != scene::component::TilemapChunk::emptyId ? 1 : 0;
		}
	}
	oCells.resize(count);
}

void buildChunk(const TilemapLayer& iLayer, const math::vec2ui& iChunk, ChunkGpu& ioChunk, const bool iLate) {
	auto& cells = g_state->instanceScratch;
	collectChunkCells(iLayer, iChunk, cells);
	ioChunk.generation = iLayer.getChunkGeneration(iChunk);
	ioChunk.instanceCount = static_cast<uint32_t>(cells.size());
	++g_state->prepareStats.builtChunkCount;
	if (iLate)
		++g_state->prepareStats.lateBuiltChunkCount;
	if (cells.empty()) {
		ioChunk.drawData.reset();
		ioChunk.capacity = 0;
		return;
	}
	if (!ioChunk.drawData || ioChunk.capacity < ioChunk.instanceCount) {
		ioChunk.capacity = std::min(g_chunkCells, std::bit_ceil(std::max(ioChunk.instanceCount, g_minChunkCapacity)));
		ioChunk.drawData = makeChunkDrawData(ioChunk.capacity);
	}
	const auto bytes = static_cast<uint32_t>(cells.size() * sizeof(CellInstance));
	ioChunk.drawData->setInstanceData(cells.data(), bytes);
	g_state->prepareStats.uploadedBytes += bytes;
}

void syncLayer(const TilemapLayer& iLayer, LayerGpu& ioLayer, const bool iLate) {
	if (ioLayer.generation == iLayer.getGeneration() && ioLayer.size == iLayer.getSize())
		return;
	const auto chunkGrid = iLayer.getChunkGridSize();
	if (ioLayer.size != iLayer.getSize() || ioLayer.chunkGrid != chunkGrid) {
		ioLayer.chunks.clear();
		ioLayer.chunks.resize(static_cast<size_t>(chunkGrid.x()) * chunkGrid.y());
		ioLayer.size = iLayer.getSize();
		ioLayer.chunkGrid = chunkGrid;
	}
	ioLayer.occupiedChunkCount = 0;
	for (uint32_t cy = 0; cy < chunkGrid.y(); ++cy) {
		for (uint32_t cx = 0; cx < chunkGrid.x(); ++cx) {
			auto& chunk = ioLayer.chunks[static_cast<size_t>(cy) * chunkGrid.x() + cx];
			if (chunk.generation != iLayer.getChunkGeneration({cx, cy}))
				buildChunk(iLayer, {cx, cy}, chunk, iLate);
			if (chunk.instanceCount > 0)
				++ioLayer.occupiedChunkCount;
		}
	}
	ioLayer.generation = iLayer.getGeneration();
}

auto syncAsset(const scene::TilemapAsset& iAsset, const bool iLate) -> AssetGpu& {
	auto& entry = g_state->cache[&iAsset];
	entry.lastUsedPrepare = g_state->prepareIndex;
	if (entry.layers.size() != iAsset.layers.size())
		entry.layers.resize(iAsset.layers.size());
	for (size_t i = 0; i < iAsset.layers.size(); ++i) syncLayer(iAsset.layers[i], entry.layers[i], iLate);
	return entry;
}

auto slotFor(const scene::Tileset* iTileset) -> int {
	auto& slots = g_state->tilesetSlots;
	if (const auto it = std::ranges::find(slots, iTileset); it != slots.end())
		return static_cast<int>(it - slots.begin());
	if (slots.size() >= g_maxTextureSlots)
		return -1;
	slots.push_back(iTileset);
	return static_cast<int>(slots.size() - 1);
}

// True when the four clip-space corners all lie beyond one side of the view volume (x or y).
auto isOutsideView(const std::array<math::vec4, 4>& iClip) -> bool {
	for (const auto& corner: iClip) {
		if (corner.w() <= 0.f)
			return false;
	}
	const auto allBeyond = [&iClip](const size_t iAxis, const float iSign) -> bool {
		return std::ranges::all_of(iClip, [iAxis, iSign](const math::vec4& iCorner) -> bool {
			return iSign * iCorner[iAxis] > iCorner.w();
		});
	};
	return allBeyond(0, 1.f) || allBeyond(0, -1.f) || allBeyond(1, 1.f) || allBeyond(1, -1.f);
}

struct LayerDraw {
	math::mat4 modelViewProjection;
	math::vec2 gridOrigin;
	float cellSize = 1.f;
	float depth = 0.f;
};

auto isChunkVisible(const LayerDraw& iDraw, const LayerGpu& iLayer, const math::vec2ui& iChunk) -> bool {
	const float half = 0.5f * iDraw.cellSize;
	const auto x0 = static_cast<float>(iChunk.x() * TilemapLayer::chunkSize);
	const auto y0 = static_cast<float>(iChunk.y() * TilemapLayer::chunkSize);
	const auto x1 = static_cast<float>(std::min(iLayer.size.x(), (iChunk.x() + 1) * TilemapLayer::chunkSize) - 1);
	const auto y1 = static_cast<float>(std::min(iLayer.size.y(), (iChunk.y() + 1) * TilemapLayer::chunkSize) - 1);
	const float left = iDraw.gridOrigin.x() + x0 * iDraw.cellSize - half;
	const float right = iDraw.gridOrigin.x() + x1 * iDraw.cellSize + half;
	const float top = iDraw.gridOrigin.y() - y0 * iDraw.cellSize + half;
	const float bottom = iDraw.gridOrigin.y() - y1 * iDraw.cellSize - half;
	const auto& mvp = iDraw.modelViewProjection;
	return !isOutsideView(
			{mvp * math::vec4{left, bottom, iDraw.depth, 1.f}, mvp * math::vec4{right, bottom, iDraw.depth, 1.f},
			 mvp * math::vec4{right, top, iDraw.depth, 1.f}, mvp * math::vec4{left, top, iDraw.depth, 1.f}});
}

void drawLayer(const LayerDraw& iDraw, const LayerGpu& iLayer) {
	for (uint32_t cy = 0; cy < iLayer.chunkGrid.y(); ++cy) {
		for (uint32_t cx = 0; cx < iLayer.chunkGrid.x(); ++cx) {
			const auto& chunk = iLayer.chunks[static_cast<size_t>(cy) * iLayer.chunkGrid.x() + cx];
			if (chunk.instanceCount == 0 || !chunk.drawData)
				continue;
			if (!isChunkVisible(iDraw, iLayer, {cx, cy})) {
				++g_state->stats.culledChunkCount;
				continue;
			}
			gpu::RenderCommand::drawDataInstanced(chunk.drawData, /*iIndexCount=*/6u, chunk.instanceCount);
			++g_state->stats.drawCallCount;
			g_state->stats.instanceCount += chunk.instanceCount;
		}
	}
}

void drawPending(const PendingTilemap& iPending, const AssetGpu& iEntry, const int iTextureSlot) {
	const auto& asset = *iPending.asset;
	const auto& tileset = *asset.tileset;
	const float cellSize = asset.cellSize;
	const float atlasW = static_cast<float>(tileset.columns) * static_cast<float>(std::max(1u, tileset.tileWidth));
	const float atlasH = static_cast<float>(tileset.rows) * static_cast<float>(std::max(1u, tileset.tileHeight));
	const math::mat4 model = iPending.transform();

	auto& ubo = g_state->drawBuffer;
	ubo.viewProjection = g_state->viewProjection;
	ubo.model = model;
	ubo.flags[0] = gpu::Framebuffer::isLinearTargetBound() ? 1 : 0;
	const auto& lighting = utils::getLighting2DState();
	ubo.flags[1] = lighting.lightCount;
	ubo.ambient = lighting.ambient;
	ubo.atlas = {static_cast<int32_t>(std::max(1u, tileset.columns)), static_cast<int32_t>(std::max(1u, tileset.rows)),
				 iPending.entityId, iTextureSlot};

	LayerDraw draw;
	draw.gridOrigin = {-static_cast<float>(asset.width - 1) * 0.5f * cellSize,
					   static_cast<float>(asset.height - 1) * 0.5f * cellSize};
	draw.cellSize = cellSize;
	for (size_t i = 0; i < asset.layers.size(); ++i) {
		++g_state->stats.layerCount;
		const auto& layer = asset.layers[i];
		if (!layer.visible)
			continue;
		const auto& gpuLayer = iEntry.layers[i];
		const auto drawDepth = draw.depth;
		draw.depth += g_layerDepthStep;
		if (gpuLayer.occupiedChunkCount == 0)
			continue;
		const math::vec2 offset{g_state->cameraPosition.x() * (1.f - layer.parallax.x()),
								g_state->cameraPosition.y() * (1.f - layer.parallax.y())};
		const math::mat4 parallaxShift =
				math::translate(math::identity<float, 4>(), math::vec3{offset.x(), offset.y(), 0.f});
		const LayerDraw layerDraw{.modelViewProjection = g_state->viewProjection * parallaxShift * model,
								  .gridOrigin = draw.gridOrigin,
								  .cellSize = cellSize,
								  .depth = drawDepth};
		ubo.grid = {draw.gridOrigin.x(), draw.gridOrigin.y(), cellSize, drawDepth};
		ubo.texel = {0.5f / std::max(1.f, atlasW), 0.5f / std::max(1.f, atlasH), offset.x(), offset.y()};
		g_state->drawUbo->setData(&ubo, sizeof(DrawUbo), 0);
		drawLayer(layerDraw, gpuLayer);
	}
}

}// namespace

void RendererTilemap::init() {
	if (isReady())
		return;
	g_state = mkShared<InternalState>();

	const std::array<gpu::BindingDecl, 4> tilemapBindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::VertexFragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = 32,
							 .stages = gpu::ShaderStage::Fragment},
			// The 2D lights at the bindings of Renderer2D: on OpenGL the storage binding points are global.
			gpu::BindingDecl{.binding = 5,
							 .type = gpu::BindingType::StorageBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 6,
							 .type = gpu::BindingType::StorageBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
	};
	gpu::RendererDescriptors::declare(g_rendererKey, tilemapBindings);

	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	g_state->drawUbo = gpu::UniformBuffer::create(sizeof(DrawUbo), 0, g_rendererKey);
	g_state->initialized = true;
	OWL_CORE_TRACE("RendererTilemap: pipeline ready ({}² cells per chunk).", TilemapLayer::chunkSize)
}

void RendererTilemap::shutdown() {
	if (!g_state)
		return;
	g_state.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

void RendererTilemap::beginPrepare() {
	if (!isReady())
		return;
	++g_state->prepareIndex;
	g_state->prepareStats = PrepareStatistics{};
	std::erase_if(g_state->cache, [](const auto& iEntry) -> bool {
		return iEntry.second.lastUsedPrepare + cacheEvictionPrepares < g_state->prepareIndex;
	});
}

void RendererTilemap::prepareTilemap(const scene::TilemapAsset& iAsset) {
	OWL_PROFILE_FUNCTION()

	if (!isReady() || iAsset.layers.empty())
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	syncAsset(iAsset, false);
}

void RendererTilemap::beginScene(const Camera& iCamera) {
	if (!isReady())
		return;
	resetStats();
	g_state->pending.clear();
	g_state->viewProjection = iCamera.getViewProjection();
	const math::vec4 eye = math::inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
	g_state->cameraPosition = {eye.x(), eye.y()};
}

void RendererTilemap::endScene() {
	// No-op: queued tilemaps are emitted by `flushPending` inside the `Renderer2D` render-pass batch.
}

void RendererTilemap::drawTilemap(const scene::TilemapAsset& iAsset, const math::Transform& iWorldTransform,
								  const int iEntityId) {
	if (!isReady())
		return;
	if (!iAsset.tileset || iAsset.layers.empty())
		return;
	if (!iAsset.tileset->texture)
		return;
	g_state->pending.push_back({.asset = &iAsset, .transform = iWorldTransform, .entityId = iEntityId});
}

void RendererTilemap::flushPending() {
	OWL_PROFILE_FUNCTION()

	if (!isReady() || g_state->pending.empty())
		return;

	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	g_state->tilesetSlots.clear();
	for (const auto& pending: g_state->pending) {
		if (slotFor(pending.asset->tileset.get()) < 0)
			OWL_CORE_FRAME_TRACE("RendererTilemap: more than {} distinct tilesets in one batch.", g_maxTextureSlots)
	}
	g_state->textureScratch.clear();
	for (const auto* tileset: g_state->tilesetSlots) g_state->textureScratch.push_back(tileset->texture);
	gpu::RenderCommand::bindTextures(g_state->textureScratch);
	if (const auto& lighting = utils::getLighting2DState();
		lighting.ambient.w() != 0.f && lighting.lights && lighting.shadows) {
		lighting.lights->bind(/*iBinding=*/5u);
		lighting.shadows->bind(/*iBinding=*/6u);
	}

	for (const auto& pending: g_state->pending) {
		const int textureSlot = slotFor(pending.asset->tileset.get());
		if (textureSlot < 0)
			continue;
		const auto& entry = syncAsset(*pending.asset, true);
		drawPending(pending, entry, textureSlot);
	}
	g_state->pending.clear();
}

void RendererTilemap::clearCache() {
	if (g_state)
		g_state->cache.clear();
}

auto RendererTilemap::getCachedAssetCount() -> size_t { return g_state ? g_state->cache.size() : 0; }

auto RendererTilemap::getStatistics() -> Statistics {
	if (!g_state)
		return {};
	return g_state->stats;
}

auto RendererTilemap::getPrepareStatistics() -> PrepareStatistics {
	if (!g_state)
		return {};
	return g_state->prepareStats;
}

void RendererTilemap::resetStats() {
	if (!g_state)
		return;
	g_state->stats = Statistics{};
}

}// namespace owl::renderer
