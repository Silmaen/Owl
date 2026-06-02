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
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"

namespace owl::renderer {

namespace {

constexpr uint32_t kMaxInstancesPerDraw = 1u << 14u;

struct CameraUbo {
	math::mat4 viewProjection;
};

struct CellInstance {
	math::vec2 cellWorld{0.f, 0.f};
	int32_t tileIndex = -1;
	int32_t entityId = -1;
	float layerZ = 0.f;
	float cellSize = 1.f;
	int32_t atlasColumns = 1;
	int32_t atlasRows = 1;
	math::vec2 halfTexel{0.f, 0.f};
	int32_t textureSlot = 0;
};

struct PendingTilemap {
	const scene::TilemapAsset* asset = nullptr;
	math::Transform transform;
	int entityId = -1;
};

struct InternalState {
	bool initialized = false;
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> cameraUbo;
	CameraUbo cameraBuffer;
	std::vector<CellInstance> instanceScratch;
	std::vector<PendingTilemap> pending;
	RendererTilemap::Statistics stats;
};

shared<InternalState> g_state;

}// namespace

void RendererTilemap::init() {
	if (g_state && g_state->initialized)
		return;
	g_state = mkShared<InternalState>();

	const std::array<gpu::BindingDecl, 2> tilemapBindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Vertex},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = 32,
							 .stages = gpu::ShaderStage::Fragment},
	};
	gpu::RendererDescriptors::declare("tilemap_instanced", tilemapBindings);

	const gpu::RendererDescriptors::ScopedActive scoped{"tilemap_instanced"};

	const gpu::BufferLayout vertexLayout{
			{"i_CornerIndex", gpu::ShaderDataType::Int},
	};
	const gpu::BufferLayout instanceLayout{
			{"i_CellWorld", gpu::ShaderDataType::Float2}, {"i_TileIndex", gpu::ShaderDataType::Int},
			{"i_EntityID", gpu::ShaderDataType::Int},     {"i_LayerZ", gpu::ShaderDataType::Float},
			{"i_CellSize", gpu::ShaderDataType::Float},   {"i_AtlasColumns", gpu::ShaderDataType::Int},
			{"i_AtlasRows", gpu::ShaderDataType::Int},    {"i_HalfTexel", gpu::ShaderDataType::Float2},
			{"i_TextureSlot", gpu::ShaderDataType::Int},
	};

	std::vector<uint32_t> quadIndices = {0, 1, 2, 2, 3, 0};

	g_state->drawData = gpu::DrawData::create();
	g_state->drawData->initInstanced(vertexLayout, instanceLayout, /*iVertexCapacity=*/4u, kMaxInstancesPerDraw,
									 "tilemap_instanced", quadIndices, "tilemap_instanced");

	constexpr std::array<int32_t, 4> cornerIndices{0, 1, 2, 3};
	g_state->drawData->setVertexData(cornerIndices.data(),
									 static_cast<uint32_t>(cornerIndices.size() * sizeof(int32_t)));

	g_state->cameraUbo = gpu::UniformBuffer::create(sizeof(CameraUbo), 0, "tilemap_instanced");
	g_state->initialized = true;
	OWL_CORE_TRACE("RendererTilemap: pipeline ready (instanced quad, capacity {} cells/draw).", kMaxInstancesPerDraw)
}

void RendererTilemap::shutdown() {
	if (!g_state)
		return;
	g_state.reset();
	gpu::RendererDescriptors::release("tilemap_instanced");
}

void RendererTilemap::beginScene(const Camera& iCamera) {
	if (!g_state || !g_state->initialized)
		return;
	resetStats();
	g_state->pending.clear();
	const gpu::RendererDescriptors::ScopedActive scoped{"tilemap_instanced"};
	g_state->cameraBuffer.viewProjection = iCamera.getViewProjection();
	g_state->cameraUbo->setData(&g_state->cameraBuffer, sizeof(CameraUbo), 0);
}

void RendererTilemap::endScene() {
	// No-op: queued tilemaps are emitted by `flushPending` inside the `Renderer2D` render-pass batch.
}

namespace {

struct CellParams {
	float originX = 0.f;
	float originY = 0.f;
	int entityId = -1;
	float layerZ = 0.f;
	int atlasColumns = 1;
	int atlasRows = 1;
	math::vec2 halfTexel{0.f, 0.f};
	int textureSlot = 0;
};

void appendLayer(const scene::TilemapAsset& iAsset, const scene::component::TilemapLayer& iLayer,
				 const CellParams& iParams, std::vector<CellInstance>& ioScratch) {
	const float cellSize = iAsset.cellSize;
	const size_t layerCellCount = iLayer.tiles.size();
	const uint32_t width = iAsset.width;
	const uint32_t height = iAsset.height;
	for (uint32_t y = 0; y < height; ++y) {
		const float cellY = iParams.originY - static_cast<float>(y) * cellSize;
		const size_t rowOffset = static_cast<size_t>(y) * width;
		for (uint32_t x = 0; x < width; ++x) {
			const size_t flat = rowOffset + x;
			if (flat >= layerCellCount)
				continue;
			const int32_t tileIdx = iLayer.tiles[flat];
			if (tileIdx < 0)
				continue;
			ioScratch.push_back({.cellWorld = {iParams.originX + static_cast<float>(x) * cellSize, cellY},
								 .tileIndex = tileIdx,
								 .entityId = iParams.entityId,
								 .layerZ = iParams.layerZ,
								 .cellSize = cellSize,
								 .atlasColumns = iParams.atlasColumns,
								 .atlasRows = iParams.atlasRows,
								 .halfTexel = iParams.halfTexel,
								 .textureSlot = iParams.textureSlot});
		}
	}
}

}// namespace

void RendererTilemap::drawTilemap(const scene::TilemapAsset& iAsset, const math::Transform& iWorldTransform,
								  const int iEntityId) {
	if (!g_state || !g_state->initialized)
		return;
	if (!iAsset.tileset || iAsset.layers.empty())
		return;
	if (!iAsset.tileset->texture)
		return;
	g_state->pending.push_back({.asset = &iAsset, .transform = iWorldTransform, .entityId = iEntityId});
}

void RendererTilemap::flushPending() {
	if (!g_state || !g_state->initialized || g_state->pending.empty())
		return;

	const gpu::RendererDescriptors::ScopedActive scoped{"tilemap_instanced"};

	constexpr size_t kMaxTextureSlots = 32;
	std::vector<const scene::Tileset*> slots;
	const auto slotFor = [&slots](const scene::Tileset* iTileset) -> int {
		for (size_t i = 0; i < slots.size(); ++i) {
			if (slots[i] == iTileset)
				return static_cast<int>(i);
		}
		if (slots.size() >= kMaxTextureSlots)
			return -1;
		slots.push_back(iTileset);
		return static_cast<int>(slots.size() - 1);
	};

	// Combine all queued tilemaps (every entity + layer) into one instance buffer; each cell carries its own params.
	g_state->instanceScratch.clear();
	for (const auto& [asset, transform, entityId]: g_state->pending) {
		const auto& assetRef = *asset;
		const auto& tileset = *assetRef.tileset;
		const int textureSlot = slotFor(&tileset);
		if (textureSlot < 0) {
			OWL_CORE_WARN("RendererTilemap: more than {} distinct tilesets in one batch; skipping a tilemap.",
						  kMaxTextureSlots)
			continue;
		}
		const float cellSize = assetRef.cellSize;
		const float atlasW = static_cast<float>(tileset.columns) * static_cast<float>(std::max(1u, tileset.tileWidth));
		const float atlasH = static_cast<float>(tileset.rows) * static_cast<float>(std::max(1u, tileset.tileHeight));

		CellParams params;
		params.originX = transform.translation().x() - static_cast<float>(assetRef.width - 1) * 0.5f * cellSize;
		params.originY = transform.translation().y() + static_cast<float>(assetRef.height - 1) * 0.5f * cellSize;
		params.entityId = entityId;
		params.atlasColumns = static_cast<int>(std::max(1u, tileset.columns));
		params.atlasRows = static_cast<int>(std::max(1u, tileset.rows));
		params.halfTexel = {0.5f / std::max(1.f, atlasW), 0.5f / std::max(1.f, atlasH)};
		params.textureSlot = textureSlot;

		params.layerZ = transform.translation().z();
		for (const auto& layer: assetRef.layers) {
			++g_state->stats.layerCount;
			if (!layer.visible)
				continue;
			appendLayer(assetRef, layer, params, g_state->instanceScratch);
			params.layerZ += 1e-4f;
		}
	}

	const auto instanceCount = static_cast<uint32_t>(g_state->instanceScratch.size());
	if (instanceCount == 0) {
		g_state->pending.clear();
		return;
	}
	if (instanceCount > kMaxInstancesPerDraw) {
		OWL_CORE_WARN("RendererTilemap: {} cells > capacity {}; truncating.", instanceCount, kMaxInstancesPerDraw)
	}
	const uint32_t drawn = std::min(instanceCount, kMaxInstancesPerDraw);

	gpu::RenderCommand::beginTextureLoad();
	for (size_t i = 0; i < slots.size(); ++i) slots[i]->texture->bind(static_cast<uint32_t>(i));
	gpu::RenderCommand::endTextureLoad();

	g_state->drawData->setInstanceData(g_state->instanceScratch.data(),
									   static_cast<uint32_t>(drawn * sizeof(CellInstance)));
	gpu::RenderCommand::drawDataInstanced(g_state->drawData, /*iIndexCount=*/6u, drawn);
	++g_state->stats.drawCallCount;
	g_state->stats.instanceCount += drawn;

	g_state->pending.clear();
}

auto RendererTilemap::getStatistics() -> Statistics {
	if (!g_state)
		return {};
	return g_state->stats;
}

void RendererTilemap::resetStats() {
	if (!g_state)
		return;
	g_state->stats = Statistics{};
}

}// namespace owl::renderer
