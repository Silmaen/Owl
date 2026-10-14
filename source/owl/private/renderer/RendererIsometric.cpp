/**
 * @file RendererIsometric.cpp
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererIsometric.h"

#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/UniformBuffer.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/component/Tilemap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace owl::renderer {

auto worldToScreen(const math::vec3& iWorld, const IsometricConfig& iConfig) -> math::vec2 {
	const auto halfW = static_cast<float>(iConfig.tileSize.x()) * 0.5f;
	const auto halfH = static_cast<float>(iConfig.tileSize.y()) * 0.5f;
	return math::vec2{iConfig.origin.x() + (iWorld.x() - iWorld.y()) * halfW,
					  iConfig.origin.y() + (iWorld.x() + iWorld.y()) * halfH - iWorld.z() * iConfig.zStep};
}

auto screenToWorld(const math::vec2& iScreen, const float iWorldZ, const IsometricConfig& iConfig) -> math::vec3 {
	const auto halfW = static_cast<float>(iConfig.tileSize.x()) * 0.5f;
	const auto halfH = static_cast<float>(iConfig.tileSize.y()) * 0.5f;
	const float diff = (iScreen.x() - iConfig.origin.x()) / halfW;
	const float sum = (iScreen.y() - iConfig.origin.y() + iWorldZ * iConfig.zStep) / halfH;
	return math::vec3{(diff + sum) * 0.5f, (sum - diff) * 0.5f, iWorldZ};
}

auto getIsometricPitch(const IsometricConfig& iConfig) -> float {
	const auto width = static_cast<float>(std::max(1u, iConfig.tileSize.x()));
	const float ratio = std::clamp(static_cast<float>(iConfig.tileSize.y()) / width, 0.01f, 0.99f);
	return std::asin(ratio);
}

// ---------------------------------------------------------------- IsometricCamera

IsometricCamera::IsometricCamera() { recalculate(); }

IsometricCamera::IsometricCamera(const IsometricConfig& iConfig) : m_config{iConfig} { recalculate(); }

IsometricCamera::~IsometricCamera() = default;

void IsometricCamera::setConfig(const IsometricConfig& iConfig) {
	m_config = iConfig;
	recalculate();
}

void IsometricCamera::setTarget(const math::vec3& iTarget) {
	m_target = iTarget;
	recalculate();
}

void IsometricCamera::setZoomLevel(const int32_t iLevel) {
	m_zoomLevel = std::clamp(iLevel, minZoomLevel, maxZoomLevel);
	recalculate();
}

auto IsometricCamera::getZoom() const -> float { return std::ldexp(1.f, m_zoomLevel); }

void IsometricCamera::setViewportSize(const math::vec2ui& iSize) {
	if (iSize.x() == 0 || iSize.y() == 0)
		return;
	m_viewportSize = iSize;
	recalculate();
}

void IsometricCamera::pan(const math::vec2& iPixelDelta) {
	const math::vec2 centre{static_cast<float>(m_viewportSize.x()) * 0.5f,
							static_cast<float>(m_viewportSize.y()) * 0.5f};
	setTarget(viewportToWorld(centre - iPixelDelta, m_target.z()));
}

auto IsometricCamera::worldToViewport(const math::vec3& iWorld) const -> math::vec2 {
	const math::vec2 centre{static_cast<float>(m_viewportSize.x()) * 0.5f,
							static_cast<float>(m_viewportSize.y()) * 0.5f};
	const IsometricConfig local{.tileSize = m_config.tileSize, .zStep = m_config.zStep, .origin = {0.f, 0.f}};
	return centre + (worldToScreen(iWorld, local) - worldToScreen(m_target, local)) * getZoom();
}

auto IsometricCamera::viewportToWorld(const math::vec2& iPixel, const float iWorldZ) const -> math::vec3 {
	const math::vec2 centre{static_cast<float>(m_viewportSize.x()) * 0.5f,
							static_cast<float>(m_viewportSize.y()) * 0.5f};
	const IsometricConfig local{.tileSize = m_config.tileSize, .zStep = m_config.zStep, .origin = {0.f, 0.f}};
	const math::vec2 screen = worldToScreen(m_target, local) + (iPixel - centre) / getZoom();
	return screenToWorld(screen, iWorldZ, local);
}

auto IsometricCamera::getViewDepth(const math::vec3& iWorld) const -> float {
	const math::vec4 viewPoint = m_view * math::vec4{iWorld.x(), iWorld.y(), iWorld.z(), 1.f};
	return viewPoint.z();
}

auto IsometricCamera::makeView(const IsometricConfig& iConfig, const math::vec3& iTarget) -> math::mat4 {
	const float pitch = getIsometricPitch(iConfig);
	const float halfW = static_cast<float>(iConfig.tileSize.x()) * 0.5f;
	const float halfH = halfW * std::sin(pitch);
	const float depthPerTile = halfW * std::cos(pitch);
	// Rows: right (x - y), up (-(x + y) and z), towards the viewer (x + y and z); world Y mirrored, Z scaled by zStep.
	math::mat4 view = math::identity<float, 4>();
	view(0, 0) = halfW;
	view(0, 1) = -halfW;
	view(0, 2) = 0.f;
	view(1, 0) = -halfH;
	view(1, 1) = -halfH;
	view(1, 2) = iConfig.zStep;
	view(2, 0) = depthPerTile;
	view(2, 1) = depthPerTile;
	view(2, 2) = iConfig.zStep * std::tan(pitch);
	for (size_t row = 0; row < 3; ++row)
		view(row, 3) = -(view(row, 0) * iTarget.x() + view(row, 1) * iTarget.y() + view(row, 2) * iTarget.z());
	return view;
}

auto IsometricCamera::makeProjection(const math::vec2ui& iViewport, const float iZoom,
									 const CameraOrtho::ClipSpace iClipSpace) -> math::mat4 {
	const float halfWidth = static_cast<float>(std::max(1u, iViewport.x())) * 0.5f / iZoom;
	const float halfHeight = static_cast<float>(std::max(1u, iViewport.y())) * 0.5f / iZoom;
	return CameraOrtho::makeProjection(-halfWidth, halfWidth, -halfHeight, halfHeight, -depthRange, depthRange,
									   iClipSpace);
}

void IsometricCamera::recalculate() {
	m_view = makeView(m_config, m_target);
	m_projection = makeProjection(m_viewportSize, getZoom(), CameraOrtho::getActiveClipSpace());
	m_viewProjection = m_projection * m_view;
}

// ---------------------------------------------------------------- Helpers

namespace {

struct DepthKey {
	float depth = 0.f;
	float bias = 0.f;
	uint32_t index = 0;
};

auto isFarther(const DepthKey& iFirst, const DepthKey& iSecond) -> bool {
	if (iFirst.depth < iSecond.depth || iSecond.depth < iFirst.depth)
		return iFirst.depth < iSecond.depth;
	return iFirst.bias < iSecond.bias;
}

}// namespace

auto getBackToFrontOrder(const IsometricCamera& iCamera, const std::span<const IsometricBillboard> iBillboards)
		-> std::vector<uint32_t> {
	std::vector<DepthKey> keys;
	keys.reserve(iBillboards.size());
	for (uint32_t index = 0; index < iBillboards.size(); ++index) {
		const auto& billboard = iBillboards[index];
		keys.push_back({.depth = iCamera.getViewDepth(billboard.foot), .bias = billboard.depthBias, .index = index});
	}
	std::ranges::stable_sort(keys, isFarther);
	std::vector<uint32_t> order;
	order.reserve(keys.size());
	for (const auto& key: keys) order.push_back(key.index);
	return order;
}

auto pickIsometricCell(const IsometricCamera& iCamera, const math::vec2& iPixel, const math::vec3& iMapOrigin,
					   const math::vec2ui& iMapSize) -> std::optional<math::vec2ui> {
	const math::vec3 ground = iCamera.viewportToWorld(iPixel, iMapOrigin.z());
	const float cellX = std::floor(ground.x() - iMapOrigin.x());
	const float cellY = std::floor(ground.y() - iMapOrigin.y());
	if (cellX < 0.f || cellY < 0.f || cellX >= static_cast<float>(iMapSize.x()) ||
		cellY >= static_cast<float>(iMapSize.y()))
		return std::nullopt;
	return math::vec2ui{static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY)};
}

// ---------------------------------------------------------------- RendererIsometric

namespace {

constexpr auto g_rendererKey = "RendererIsometric";
constexpr auto g_shaderFolder = "isometric";
constexpr auto g_shaderName = "billboard";
constexpr uint32_t g_maxTextureSlots = 32;
constexpr float g_cutoutAlpha = 0.5f;
constexpr float g_translucentDiscardAlpha = 0.001f;
constexpr uint16_t g_whiteTexture = 0xFFFF;

// Mirrored by `BillboardInstance` in `billboard.slang` (std430).
struct GpuInstance {
	math::vec4 foot;
	math::vec4 size;
	std::array<math::vec2, 4> uv;
	math::vec4 color;
	uint32_t texIndex = 0;
	int32_t entityId = -1;
	uint32_t pad0 = 0;
	uint32_t pad1 = 0;
};
static_assert(sizeof(GpuInstance) == 96, "GpuInstance must match the shader std430 layout");

// Mirrored by `IsometricScene` in `billboard.slang`.
struct SceneUbo {
	math::mat4 view = math::identity<float, 4>();
	math::mat4 projection = math::identity<float, 4>();
	math::vec4 params{0.f, 0.f, 0.f, 0.f};
};

struct Queued {
	GpuInstance gpu;
	uint16_t frameTexture = g_whiteTexture;
};

struct InternalState {
	bool initialized = false;
	shared<gpu::DrawData> opaqueDraw;
	shared<gpu::DrawData> translucentDraw;
	shared<gpu::StorageBuffer> instances;
	shared<gpu::UniformBuffer> sceneUbo;
	shared<gpu::Texture2D> whiteTexture;
	SceneUbo scene;
	IsometricCamera camera;
	float uprightSlope = 0.f;
	float flatSlope = 0.f;
	std::vector<Queued> opaque;
	std::vector<Queued> translucent;
	std::vector<DepthKey> translucentKeys;
	std::vector<shared<gpu::Texture2D>> frameTextures;
	const gpu::Texture2D* lastTexture = nullptr;
	uint16_t lastFrameTexture = g_whiteTexture;
	std::vector<GpuInstance> batch;
	std::vector<shared<gpu::Texture2D>> slots;
	std::vector<int32_t> slotOfFrameTexture;
	RendererIsometric::Statistics stats;
};

shared<InternalState> g_state;

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

auto isReady() -> bool { return g_state && g_state->initialized; }

auto makeDraw(const gpu::PipelineState& iState) -> shared<gpu::DrawData> {
	std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};
	auto draw = gpu::DrawData::create();
	draw->init({{"i_CornerIndex", gpu::ShaderDataType::Int}}, g_shaderFolder, indices, g_shaderName, iState);
	constexpr std::array<int32_t, 4> corners{0, 1, 2, 3};
	draw->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(int32_t)));
	return draw;
}

auto frameTextureOf(const shared<gpu::Texture2D>& iTexture) -> uint16_t {
	if (!iTexture)
		return g_whiteTexture;
	auto& state = *g_state;
	if (iTexture.get() == state.lastTexture)
		return state.lastFrameTexture;
	const auto it = std::ranges::find(state.frameTextures, iTexture);
	uint16_t index = 0;
	if (it != state.frameTextures.end()) {
		index = static_cast<uint16_t>(it - state.frameTextures.begin());
	} else {
		if (state.frameTextures.size() >= g_whiteTexture)
			return g_whiteTexture;
		index = static_cast<uint16_t>(state.frameTextures.size());
		state.frameTextures.push_back(iTexture);
	}
	state.lastTexture = iTexture.get();
	state.lastFrameTexture = index;
	return index;
}

void queue(const IsometricBillboard& iBillboard) {
	auto& state = *g_state;
	Queued item;
	item.gpu.foot = math::vec4{iBillboard.foot.x(), iBillboard.foot.y(), iBillboard.foot.z(), iBillboard.depthBias};
	item.gpu.size = math::vec4{iBillboard.size.x(), iBillboard.size.y(),
							   iBillboard.flat ? state.flatSlope : state.uprightSlope, 0.f};
	item.gpu.uv = iBillboard.textureCoords;
	item.gpu.color = iBillboard.color;
	item.gpu.entityId = iBillboard.entityId;
	item.frameTexture = frameTextureOf(iBillboard.texture);
	if (iBillboard.color.w() < 1.f)
		state.translucent.push_back(item);
	else
		state.opaque.push_back(item);
}

void resetSlots() {
	auto& state = *g_state;
	state.batch.clear();
	state.slots.clear();
	state.slots.push_back(state.whiteTexture);
	state.slotOfFrameTexture.assign(state.frameTextures.size(), -1);
}

void flushBatch(const shared<gpu::DrawData>& iDraw) {
	auto& state = *g_state;
	if (state.batch.empty())
		return;
	const auto count = static_cast<uint32_t>(state.batch.size());
	state.sceneUbo->setData(&state.scene, sizeof(SceneUbo), 0);
	gpu::RenderCommand::bindTextures(state.slots);
	state.instances->setData(state.batch.data(), static_cast<uint32_t>(count * sizeof(GpuInstance)), 0);
	state.instances->bind();
	gpu::RenderCommand::drawDataInstanced(iDraw, /*iIndexCount=*/6u, count);
	++state.stats.drawCallCount;
	resetSlots();
}

void emit(const Queued& iItem, const shared<gpu::DrawData>& iDraw) {
	auto& state = *g_state;
	if (state.batch.size() >= RendererIsometric::maxBillboardsPerBatch)
		flushBatch(iDraw);
	uint32_t slot = 0;
	if (iItem.frameTexture != g_whiteTexture) {
		if (state.slotOfFrameTexture[iItem.frameTexture] < 0) {
			if (state.slots.size() >= g_maxTextureSlots)
				flushBatch(iDraw);
			state.slotOfFrameTexture[iItem.frameTexture] = static_cast<int32_t>(state.slots.size());
			state.slots.push_back(state.frameTextures[iItem.frameTexture]);
		}
		slot = static_cast<uint32_t>(state.slotOfFrameTexture[iItem.frameTexture]);
	}
	state.batch.push_back(iItem.gpu);
	state.batch.back().texIndex = slot;
}

void drawOpaquePass() {
	auto& state = *g_state;
	if (state.opaque.empty())
		return;
	state.scene.params = math::vec4{g_cutoutAlpha, 1.f, 0.f, 0.f};
	resetSlots();
	for (const auto& item: state.opaque) emit(item, state.opaqueDraw);
	flushBatch(state.opaqueDraw);
	state.stats.opaqueCount += static_cast<uint32_t>(state.opaque.size());
}

void drawTranslucentPass() {
	auto& state = *g_state;
	if (state.translucent.empty())
		return;
	auto& keys = state.translucentKeys;
	keys.clear();
	for (uint32_t index = 0; index < state.translucent.size(); ++index) {
		const auto& foot = state.translucent[index].gpu.foot;
		keys.push_back(
				{.depth = state.camera.getViewDepth({foot.x(), foot.y(), foot.z()}), .bias = foot.w(), .index = index});
	}
	std::ranges::stable_sort(keys, isFarther);
	state.scene.params = math::vec4{g_translucentDiscardAlpha, 0.f, 0.f, 0.f};
	resetSlots();
	for (const auto& key: keys) emit(state.translucent[key.index], state.translucentDraw);
	flushBatch(state.translucentDraw);
	state.stats.translucentCount += static_cast<uint32_t>(state.translucent.size());
}

// True when the screen box of a chunk, raised by the tallest sprite, misses the viewport.
auto isChunkOutside(const IsometricCamera& iCamera, const math::vec3& iOrigin, const math::vec2ui& iFirst,
					const math::vec2ui& iLast, const float iSpriteHeight) -> bool {
	const auto x0 = static_cast<float>(iFirst.x());
	const auto y0 = static_cast<float>(iFirst.y());
	const auto x1 = static_cast<float>(iLast.x());
	const auto y1 = static_cast<float>(iLast.y());
	const std::array corners{iCamera.worldToViewport(iOrigin + math::vec3{x0, y0, 0.f}),
							 iCamera.worldToViewport(iOrigin + math::vec3{x1, y0, 0.f}),
							 iCamera.worldToViewport(iOrigin + math::vec3{x1, y1, 0.f}),
							 iCamera.worldToViewport(iOrigin + math::vec3{x0, y1, 0.f})};
	const auto [minX, maxX] =
			std::ranges::minmax(corners | std::views::transform([](const math::vec2& iP) -> float { return iP.x(); }));
	const auto [minY, maxY] =
			std::ranges::minmax(corners | std::views::transform([](const math::vec2& iP) -> float { return iP.y(); }));
	const float raise = iSpriteHeight * iCamera.getZoom();
	const auto& viewport = iCamera.getViewportSize();
	return maxX < 0.f || minX > static_cast<float>(viewport.x()) || maxY < 0.f ||
		   minY - raise > static_cast<float>(viewport.y());
}

}// namespace

void RendererIsometric::init() {
	OWL_PROFILE_FUNCTION()

	if (isReady())
		return;
	g_state = mkShared<InternalState>();
	const std::array<gpu::BindingDecl, 3> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::VertexFragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = g_maxTextureSlots,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 2,
							 .type = gpu::BindingType::StorageBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Vertex},
	};
	gpu::RendererDescriptors::declare(g_rendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	g_state->opaqueDraw = makeDraw({.blendMode = gpu::BlendMode::Opaque, .depthTest = true, .depthWrite = true});
	g_state->translucentDraw = makeDraw({.blendMode = gpu::BlendMode::Alpha, .depthTest = true, .depthWrite = false});
	g_state->whiteTexture =
			gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {1, 1}, .format = gpu::ImageFormat::Rgba8});
	uint32_t white = 0xffffffff;
	g_state->whiteTexture->setData(&white, sizeof(uint32_t));
	g_state->sceneUbo = gpu::UniformBuffer::create(sizeof(SceneUbo), 0, g_rendererKey);
	g_state->instances = gpu::StorageBuffer::create(maxBillboardsPerBatch * sizeof(GpuInstance), 2, g_rendererKey);
	g_state->batch.reserve(maxBillboardsPerBatch);
	g_state->initialized = true;
}

void RendererIsometric::shutdown() {
	if (!g_state)
		return;
	g_state.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

void RendererIsometric::beginScene(const IsometricCamera& iCamera) {
	if (!isReady())
		return;
	auto& state = *g_state;
	state.camera = iCamera;
	state.scene.view = iCamera.getView();
	state.scene.projection = iCamera.getProjection();
	const float pitch = getIsometricPitch(iCamera.getConfig());
	state.uprightSlope = std::tan(pitch);
	state.flatSlope = -1.f / std::tan(pitch);
	state.opaque.clear();
	state.translucent.clear();
	state.frameTextures.clear();
	state.lastTexture = nullptr;
	state.lastFrameTexture = g_whiteTexture;
}

void RendererIsometric::drawBillboard(const IsometricBillboard& iBillboard) {
	if (!isReady())
		return;
	queue(iBillboard);
}

void RendererIsometric::drawTilemap(const scene::TilemapAsset& iAsset, const math::Transform& iWorldTransform,
									const int iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!isReady() || !iAsset.tileset || !iAsset.tileset->texture)
		return;
	using scene::component::TilemapChunk;
	using scene::component::TilemapLayer;
	auto& state = *g_state;
	const auto& tileset = *iAsset.tileset;
	const auto& camera = state.camera;
	const auto tileWidth = static_cast<float>(camera.getConfig().tileSize.x());
	const float pixelScale = tileWidth / static_cast<float>(std::max(1u, tileset.tileWidth));
	const math::vec2 spriteSize{tileWidth, static_cast<float>(tileset.tileHeight) * pixelScale};
	const math::vec3 origin = iWorldTransform.translation();
	const uint16_t frameTexture = frameTextureOf(tileset.texture);
	Queued item;
	item.frameTexture = frameTexture;
	item.gpu.size = math::vec4{spriteSize.x(), spriteSize.y(), state.flatSlope, 0.f};
	item.gpu.color = math::vec4{1.f, 1.f, 1.f, 1.f};
	item.gpu.entityId = iEntityId;
	float bias = 0.f;
	for (const auto& layer: iAsset.layers) {
		const float layerBias = bias;
		bias += tilemapLayerBias;
		if (!layer.visible)
			continue;
		const auto grid = layer.getChunkGridSize();
		const auto& size = layer.getSize();
		for (uint32_t cy = 0; cy < grid.y(); ++cy) {
			for (uint32_t cx = 0; cx < grid.x(); ++cx) {
				const auto* chunk = layer.getChunk({cx, cy});
				if (chunk == nullptr)
					continue;
				const math::vec2ui first{cx * TilemapLayer::chunkSize, cy * TilemapLayer::chunkSize};
				const math::vec2ui last{std::min(size.x(), first.x() + TilemapLayer::chunkSize),
										std::min(size.y(), first.y() + TilemapLayer::chunkSize)};
				if (isChunkOutside(camera, origin, first, last, spriteSize.y())) {
					++state.stats.culledChunkCount;
					continue;
				}
				for (uint32_t y = first.y(); y < last.y(); ++y) {
					const uint32_t row = (y - first.y()) * TilemapLayer::chunkSize;
					for (uint32_t x = first.x(); x < last.x(); ++x) {
						const uint16_t id = chunk->terrain[row + x - first.x()];
						if (id == TilemapChunk::emptyId)
							continue;
						item.gpu.foot = math::vec4{origin.x() + static_cast<float>(x + 1),
												   origin.y() + static_cast<float>(y + 1), origin.z(), layerBias};
						item.gpu.uv = tileset.getTileUv(id);
						state.opaque.push_back(item);
					}
				}
			}
		}
	}
}

void RendererIsometric::endScene() {
	OWL_PROFILE_FUNCTION()

	if (!isReady())
		return;
	auto& state = *g_state;
	if (state.opaque.empty() && state.translucent.empty())
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	gpu::RenderCommand::beginBatch();
	drawOpaquePass();
	drawTranslucentPass();
	gpu::RenderCommand::endBatch();
	state.opaque.clear();
	state.translucent.clear();
}

auto RendererIsometric::getStatistics() -> Statistics { return g_state ? g_state->stats : Statistics{}; }

void RendererIsometric::resetStats() {
	if (g_state)
		g_state->stats = Statistics{};
}

}// namespace owl::renderer
