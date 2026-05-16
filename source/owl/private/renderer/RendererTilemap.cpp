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
#include "renderer/gpu/UniformBuffer.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"

namespace owl::renderer {

namespace {

/**
 * @brief
 *  Maximum number of instances (cells) we can upload in a single draw.
 *  Sized generously to cover a 128×128 tilemap layer (16 384 cells) so we
 *  almost never spill into multi-draw paths. Per-instance struct is 16 B,
 *  so the buffer is 256 KB — easily within Vulkan / OpenGL VBO limits.
 */
constexpr uint32_t kMaxInstancesPerDraw = 1u << 14u;

/// Layout of the camera UBO (binding 0). Matches `quad.slang`'s `Camera`.
struct CameraUbo {
	math::mat4 viewProjection;
};

/**
 * @brief
 *  Layout of the per-draw UBO bound at `binding=2` in
 *  `tilemap_instanced.slang`. Must stay layout-compatible with the Slang
 *  struct — std140 packing for `ConstantBuffer`.
 */
struct DrawUbo {
	uint32_t atlasColumns = 1;
	uint32_t atlasRows = 1;
	float cellSize = 1.f;
	float layerZ = 0.f;
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	uint32_t textureSlot = 0;
	uint32_t _pad0 = 0;
	math::vec2 halfTexel{0.f, 0.f};
};

/**
 * @brief
 *  Per-instance attribute uploaded into the dynamic instance VBO. Layout
 *  matches `vk::location 1..3` of `vertexMain` in `tilemap_instanced.slang` —
 *  location 1 is float2 `cellWorld`, location 2 is int `tileIndex`, location
 *  3 is int `entityID`.
 */
struct CellInstance {
	math::vec2 cellWorld{0.f, 0.f};
	int32_t tileIndex = -1;
	int32_t entityId = -1;
};

/**
 * @brief
 *  Process-lifetime state for the renderer.
 */
struct InternalState {
	bool initialized = false;
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> cameraUbo;
	shared<gpu::UniformBuffer> drawUbo;
	CameraUbo cameraBuffer;
	DrawUbo drawBuffer;
	// Per-frame instance scratch buffer — `thread_local` reuse keeps the
	// vector's capacity across frames.
	std::vector<CellInstance> instanceScratch;
	RendererTilemap::Statistics stats;
};

shared<InternalState> g_state;

}// namespace

void RendererTilemap::init() {
	if (g_state && g_state->initialized)
		return;
	g_state = mkShared<InternalState>();

	// Static per-vertex buffer: 4 corners, indexed by attribute `cornerIndex`
	// (Int). Two triangles via the index buffer below.
	const gpu::BufferLayout vertexLayout{
			{"i_CornerIndex", gpu::ShaderDataType::Int},
	};
	const gpu::BufferLayout instanceLayout{
			{"i_CellWorld", gpu::ShaderDataType::Float2},
			{"i_TileIndex", gpu::ShaderDataType::Int},
			{"i_EntityID", gpu::ShaderDataType::Int},
	};

	std::vector<uint32_t> quadIndices = {0, 1, 2, 2, 3, 0};

	g_state->drawData = gpu::DrawData::create();
	g_state->drawData->initInstanced(vertexLayout, instanceLayout, /*iVertexCapacity=*/4u, kMaxInstancesPerDraw,
									 "tilemap_instanced", quadIndices, "tilemap_instanced");

	// Per-vertex data is static — 4 corner indices, uploaded once.
	const std::array<int32_t, 4> cornerIndices{0, 1, 2, 3};
	g_state->drawData->setVertexData(cornerIndices.data(),
									 static_cast<uint32_t>(cornerIndices.size() * sizeof(int32_t)));

	g_state->cameraUbo = gpu::UniformBuffer::create(sizeof(CameraUbo), 0, "tilemap_instanced");
	g_state->drawUbo = gpu::UniformBuffer::create(sizeof(DrawUbo), 2, "tilemap_instanced");
	g_state->initialized = true;
	OWL_CORE_TRACE("RendererTilemap: pipeline ready (instanced quad, capacity {} cells/draw).", kMaxInstancesPerDraw)
}

void RendererTilemap::shutdown() {
	if (!g_state)
		return;
	g_state.reset();
}

void RendererTilemap::beginScene(const Camera& iCamera) {
	if (!g_state || !g_state->initialized)
		return;
	resetStats();
	g_state->cameraBuffer.viewProjection = iCamera.getViewProjection();
	g_state->cameraUbo->setData(&g_state->cameraBuffer, sizeof(CameraUbo), 0);
}

void RendererTilemap::endScene() {
	// No-op: every `drawTilemap` issues its draws inline.
}

namespace {

/**
 * @brief
 *  Compact non-empty cells of one tilemap layer into the per-instance
 *  scratch buffer.
 * @param[in] iAsset The tilemap asset (provides cellSize and dimensions).
 * @param[in] iLayer The current layer (provides `tiles`).
 * @param[in] iOriginX World-space X of the (0,0) cell centre.
 * @param[in] iOriginY World-space Y of the (0,0) cell centre.
 * @param[in] iEntityId Entity id (propagated to every instance for picking).
 * @param[in,out] ioScratch Pre-allocated scratch buffer; resized to the
 *  exact instance count on return.
 */
void compactLayer(const scene::TilemapAsset& iAsset, const scene::component::TilemapLayer& iLayer, const float iOriginX,
				  const float iOriginY, const int iEntityId, std::vector<CellInstance>& ioScratch) {
	ioScratch.clear();
	const float cellSize = iAsset.cellSize;
	const size_t layerCellCount = iLayer.tiles.size();
	const uint32_t width = iAsset.width;
	const uint32_t height = iAsset.height;
	ioScratch.reserve(layerCellCount);
	for (uint32_t y = 0; y < height; ++y) {
		const float cellY = iOriginY - static_cast<float>(y) * cellSize;
		const size_t rowOffset = static_cast<size_t>(y) * width;
		for (uint32_t x = 0; x < width; ++x) {
			const size_t flat = rowOffset + x;
			if (flat >= layerCellCount)
				continue;
			const int32_t tileIdx = iLayer.tiles[flat];
			if (tileIdx < 0)
				continue;
			ioScratch.push_back({.cellWorld = {iOriginX + static_cast<float>(x) * cellSize, cellY},
								 .tileIndex = tileIdx,
								 .entityId = iEntityId});
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
	const auto& tileset = *iAsset.tileset;
	if (!tileset.texture)
		return;

	const float cellSize = iAsset.cellSize;
	const float originX = iWorldTransform.translation().x() - static_cast<float>(iAsset.width - 1) * 0.5f * cellSize;
	const float originY = iWorldTransform.translation().y() + static_cast<float>(iAsset.height - 1) * 0.5f * cellSize;

	// Bind the atlas texture to slot 0 of the shader's `gTextures[32]` array.
	tileset.texture->bind(0);

	// Half-texel inset matches `Tileset::getTileUv`'s anti-bleed inset so
	// instanced quads and legacy `drawTilemapQuads` produce identical
	// sampling.
	const float atlasW = static_cast<float>(tileset.columns) * static_cast<float>(std::max(1u, tileset.tileWidth));
	const float atlasH = static_cast<float>(tileset.rows) * static_cast<float>(std::max(1u, tileset.tileHeight));
	const math::vec2 halfTexel{0.5f / std::max(1.f, atlasW), 0.5f / std::max(1.f, atlasH)};

	float layerZ = iWorldTransform.translation().z();
	for (const auto& layer: iAsset.layers) {
		++g_state->stats.layerCount;
		if (!layer.visible)
			continue;
		compactLayer(iAsset, layer, originX, originY, iEntityId, g_state->instanceScratch);
		const auto instanceCount = static_cast<uint32_t>(g_state->instanceScratch.size());
		if (instanceCount == 0)
			continue;
		if (instanceCount > kMaxInstancesPerDraw) {
			OWL_CORE_WARN("RendererTilemap: layer has {} cells > capacity {}; truncating.", instanceCount,
						  kMaxInstancesPerDraw)
		}
		const uint32_t drawn = std::min(instanceCount, kMaxInstancesPerDraw);

		g_state->drawBuffer.atlasColumns = std::max(1u, tileset.columns);
		g_state->drawBuffer.atlasRows = std::max(1u, tileset.rows);
		g_state->drawBuffer.cellSize = cellSize;
		g_state->drawBuffer.layerZ = layerZ;
		g_state->drawBuffer.tint = math::vec4{1.f, 1.f, 1.f, 1.f};
		g_state->drawBuffer.textureSlot = 0;
		g_state->drawBuffer.halfTexel = halfTexel;
		g_state->drawUbo->setData(&g_state->drawBuffer, sizeof(DrawUbo), 0);

		g_state->drawData->setInstanceData(g_state->instanceScratch.data(),
										   static_cast<uint32_t>(drawn * sizeof(CellInstance)));
		gpu::RenderCommand::drawDataInstanced(g_state->drawData, /*iIndexCount=*/6u, drawn);
		++g_state->stats.drawCallCount;
		g_state->stats.instanceCount += drawn;

		// Nudge the Z slightly so subsequent layers paint on top without
		// the painter's order flipping under depth-test disabled rendering.
		layerZ += 1e-4f;
	}
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
