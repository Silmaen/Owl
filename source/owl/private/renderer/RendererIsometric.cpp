/**
 * @file RendererIsometric.cpp
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererIsometric.h"

#include "renderer/IsometricObjects.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/UniformBuffer.h"
#include "scene/TilemapAsset.h"
#include "scene/TilemapTerrain.h"
#include "scene/Tileset.h"
#include "scene/component/Tilemap.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
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

auto packColor(const math::vec4& iColor) -> uint32_t {
	const auto channel = [](const float iValue) -> uint32_t {
		return static_cast<uint32_t>(std::lround(std::clamp(iValue, 0.f, 1.f) * 255.f));
	};
	return channel(iColor.x()) | (channel(iColor.y()) << 8u) | (channel(iColor.z()) << 16u) |
		   (channel(iColor.w()) << 24u);
}

auto getSpriteScreenRect(const IsometricCamera& iCamera, const IsometricSprite& iSprite) -> math::vec4 {
	const float zoom = iCamera.getZoom();
	const math::vec2 foot = iCamera.worldToViewport(iSprite.foot);
	const float width = iSprite.size.x() * zoom;
	const float height = iSprite.size.y() * zoom;
	const float left = foot.x() - iSprite.anchor.x() * width;
	const float bottom = foot.y() + iSprite.anchor.y() * height;
	return {left, bottom - height, left + width, bottom};
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
constexpr auto g_spriteShaderName = "sprite";
constexpr auto g_tileShaderName = "tile";
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

// Mirrored by `IsometricScene` in `billboard.slang` (the first three members) and `tile.slang` (all of them).
struct SceneUbo {
	math::mat4 view = math::identity<float, 4>();
	math::mat4 projection = math::identity<float, 4>();
	math::vec4 params{0.f, 0.f, 0.f, 0.f};
	// Tilemap draws only: back corner of the map.
	math::vec4 origin{0.f, 0.f, 0.f, 0.f};
	// Tilemap draws only: sprite size in pixels, depth slope.
	math::vec4 tile{0.f, 0.f, 0.f, 0.f};
	// x: entity id (tilemap draws only); y: 1 when the bound framebuffer stores linear colour; zw: padding.
	std::array<int32_t, 4> ids{-1, 0, 0, 0};
};

// Mirrored by `SpriteInstance` in `sprite.slang` (std430).
struct GpuSprite {
	math::vec4 foot;
	math::vec4 sizeAnchor;
	math::vec4 uvRect;
	uint32_t color = 0xFFFFFFFFu;
	uint32_t texIndex = 0;
	int32_t entityId = -1;
	uint32_t pad = 0;
};
static_assert(sizeof(GpuSprite) == 64, "GpuSprite must match the shader std430 layout");

struct Queued {
	GpuInstance gpu;
	uint16_t frameTexture = g_whiteTexture;
};

struct QueuedSprite {
	GpuSprite gpu;
	uint16_t frameTexture = g_whiteTexture;
};

using scene::component::TilemapChunk;
using scene::component::TilemapLayer;

constexpr uint32_t g_chunkCells = TilemapLayer::chunkSize * TilemapLayer::chunkSize;
constexpr uint32_t g_minChunkCapacity = 64;
// A cell holds a terrain tile and an overlay: two instances at most.
constexpr uint32_t g_maxChunkInstances = 2 * g_chunkCells;

// One tile of a chunk, mirrored by the per-instance attributes of `tile.slang`.
struct TileInstance {
	// xy: foot in map space (the front corner of the cell), z: height of the lowest corner, w: depth bias.
	math::vec4 foot;
	// Texture rectangle: u of the left, v of the bottom, u of the right, v of the top.
	math::vec4 uvRect;
};
static_assert(sizeof(TileInstance) == 32, "TileInstance must match the instance layout of tile.slang");

struct ChunkGpu {
	uint64_t generation = 0;
	uint64_t heightGeneration = 0;
	uint32_t instanceCount = 0;
	uint32_t capacity = 0;
	shared<gpu::DrawData> drawData;
};

struct LayerGpu {
	math::vec2ui size{0, 0};
	math::vec2ui chunkGrid{0, 0};
	uint32_t occupiedChunkCount = 0;
	std::vector<ChunkGpu> chunks;
};

struct AssetGpu {
	std::vector<LayerGpu> layers;
	const scene::Tileset* tileset = nullptr;
	uint64_t tilesetSignature = 0;
	uint64_t lastUsedPrepare = 0;
};

struct PendingTilemap {
	const scene::TilemapAsset* asset = nullptr;
	math::vec3 origin{0.f, 0.f, 0.f};
	int entityId = -1;
};

struct InternalState {
	bool initialized = false;
	shared<gpu::DrawData> opaqueDraw;
	shared<gpu::DrawData> translucentDraw;
	shared<gpu::DrawData> spriteDraw;
	shared<gpu::StorageBuffer> instances;
	shared<gpu::StorageBuffer> spriteInstances;
	shared<gpu::UniformBuffer> sceneUbo;
	shared<gpu::Texture2D> whiteTexture;
	SceneUbo scene;
	IsometricCamera camera;
	float uprightSlope = 0.f;
	float flatSlope = 0.f;
	std::vector<Queued> opaque;
	std::vector<Queued> translucent;
	std::vector<QueuedSprite> sprites;
	std::vector<PendingTilemap> tilemaps;
	std::unordered_map<const scene::TilemapAsset*, AssetGpu> cache;
	std::vector<TileInstance> tileScratch;
	std::vector<uint32_t> quadIndices{0, 1, 2, 2, 3, 0};
	uint64_t prepareIndex = 0;
	RendererIsometric::PrepareStatistics prepareStats;
	std::vector<DepthKey> translucentKeys;
	std::vector<shared<gpu::Texture2D>> frameTextures;
	const gpu::Texture2D* lastTexture = nullptr;
	uint16_t lastFrameTexture = g_whiteTexture;
	std::vector<GpuInstance> batch;
	std::vector<GpuSprite> spriteBatch;
	float snapZoom = 0.f;
	math::vec2 snapOffset{0.f, 0.f};
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

auto makeSpriteDraw() -> shared<gpu::DrawData> {
	std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};
	auto draw = gpu::DrawData::create();
	draw->init({{"i_CornerIndex", gpu::ShaderDataType::Int}}, g_shaderFolder, indices, g_spriteShaderName,
			   {.blendMode = gpu::BlendMode::Opaque, .depthTest = true, .depthWrite = true});
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

void queueSprite(const uint16_t iFrameTexture, const IsometricSprite& iSprite) {
	auto& state = *g_state;
	const auto& viewport = state.camera.getViewportSize();
	const math::vec4 rect = getSpriteScreenRect(state.camera, iSprite);
	if (rect.z() < 0.f || rect.x() > static_cast<float>(viewport.x()) || rect.w() < 0.f ||
		rect.y() > static_cast<float>(viewport.y())) {
		++state.stats.culledSpriteCount;
		return;
	}
	QueuedSprite item;
	item.frameTexture = iFrameTexture;
	item.gpu.foot = math::vec4{iSprite.foot.x(), iSprite.foot.y(), iSprite.foot.z(), iSprite.depthBias};
	item.gpu.sizeAnchor = math::vec4{iSprite.size.x(), iSprite.size.y(), iSprite.anchor.x(), iSprite.anchor.y()};
	item.gpu.uvRect = iSprite.uvRect;
	item.gpu.color = iSprite.color;
	item.gpu.entityId = iSprite.entityId;
	state.sprites.push_back(item);
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
	state.spriteBatch.clear();
	state.slots.clear();
	state.slots.push_back(state.whiteTexture);
	state.slotOfFrameTexture.assign(state.frameTextures.size(), -1);
}

void flushBatch(const shared<gpu::DrawData>& iDraw) {
	auto& state = *g_state;
	if (state.batch.empty())
		return;
	const auto count = static_cast<uint32_t>(state.batch.size());
	state.scene.ids[1] = gpu::Framebuffer::isLinearTargetBound() ? 1 : 0;
	state.sceneUbo->setData(&state.scene, sizeof(SceneUbo), 0);
	gpu::RenderCommand::bindTextures(state.slots);
	state.instances->setData(state.batch.data(), static_cast<uint32_t>(count * sizeof(GpuInstance)), 0);
	state.instances->bind();
	gpu::RenderCommand::drawDataInstanced(iDraw, /*iIndexCount=*/6u, count);
	++state.stats.drawCallCount;
	resetSlots();
}

void flushSpriteBatch() {
	auto& state = *g_state;
	if (state.spriteBatch.empty())
		return;
	const auto count = static_cast<uint32_t>(state.spriteBatch.size());
	state.scene.ids[1] = gpu::Framebuffer::isLinearTargetBound() ? 1 : 0;
	state.sceneUbo->setData(&state.scene, sizeof(SceneUbo), 0);
	gpu::RenderCommand::bindTextures(state.slots);
	state.spriteInstances->setData(state.spriteBatch.data(), static_cast<uint32_t>(count * sizeof(GpuSprite)), 0);
	state.spriteInstances->bind();
	gpu::RenderCommand::drawDataInstanced(state.spriteDraw, /*iIndexCount=*/6u, count);
	++state.stats.drawCallCount;
	resetSlots();
}

void emitSprite(const QueuedSprite& iItem) {
	auto& state = *g_state;
	if (state.spriteBatch.size() >= RendererIsometric::maxSpritesPerBatch)
		flushSpriteBatch();
	uint32_t slot = 0;
	if (iItem.frameTexture != g_whiteTexture) {
		if (state.slotOfFrameTexture[iItem.frameTexture] < 0) {
			if (state.slots.size() >= g_maxTextureSlots)
				flushSpriteBatch();
			state.slotOfFrameTexture[iItem.frameTexture] = static_cast<int32_t>(state.slots.size());
			state.slots.push_back(state.frameTextures[iItem.frameTexture]);
		}
		slot = static_cast<uint32_t>(state.slotOfFrameTexture[iItem.frameTexture]);
	}
	state.spriteBatch.push_back(iItem.gpu);
	state.spriteBatch.back().texIndex = slot;
}

void setPassParams(const float iAlpha) {
	auto& state = *g_state;
	state.scene.params = math::vec4{iAlpha, state.snapZoom, state.snapOffset.x(), state.snapOffset.y()};
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
	setPassParams(g_cutoutAlpha);
	resetSlots();
	for (const auto& item: state.opaque) emit(item, state.opaqueDraw);
	flushBatch(state.opaqueDraw);
	state.stats.opaqueCount += static_cast<uint32_t>(state.opaque.size());
}

void drawSpritePass() {
	auto& state = *g_state;
	if (state.sprites.empty())
		return;
	setPassParams(g_cutoutAlpha);
	resetSlots();
	for (const auto& item: state.sprites) emitSprite(item);
	flushSpriteBatch();
	state.stats.spriteCount += static_cast<uint32_t>(state.sprites.size());
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
	setPassParams(g_translucentDiscardAlpha);
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

auto makeChunkDraw(const uint32_t iCapacity) -> shared<gpu::DrawData> {
	const gpu::BufferLayout vertexLayout{{"i_CornerIndex", gpu::ShaderDataType::Int}};
	const gpu::BufferLayout instanceLayout{{"i_Foot", gpu::ShaderDataType::Float4},
										   {"i_UvRect", gpu::ShaderDataType::Float4}};
	auto draw = gpu::DrawData::create();
	draw->initInstanced(vertexLayout, instanceLayout, /*iVertexCapacity=*/4u, iCapacity, g_shaderFolder,
						g_state->quadIndices, g_tileShaderName,
						{.blendMode = gpu::BlendMode::Opaque, .depthTest = true, .depthWrite = true});
	constexpr std::array<int32_t, 4> corners{0, 1, 2, 3};
	draw->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(int32_t)));
	g_state->prepareStats.uploadedBytes += corners.size() * sizeof(int32_t);
	return draw;
}

// Everything of a tileset that decides the slope variant and the texture rectangle of a tile.
auto getTilesetSignature(const scene::Tileset& iTileset) -> uint64_t {
	uint64_t hash = 14695981039346656037ull;
	const auto mix = [&hash](const uint64_t iValue) -> void { hash = (hash ^ iValue) * 1099511628211ull; };
	mix(iTileset.columns);
	mix(iTileset.rows);
	mix(iTileset.tileWidth);
	mix(iTileset.tileHeight);
	mix(iTileset.slopeSets.size());
	for (const auto& set: iTileset.slopeSets) {
		mix(static_cast<uint32_t>(set.id));
		for (const auto tile: set.tiles) mix(static_cast<uint32_t>(tile));
		for (const auto tile: set.foundations) mix(static_cast<uint32_t>(tile));
	}
	return hash;
}

void collectChunkTiles(const scene::TilemapAsset& iAsset, const TilemapLayer& iLayer, const math::vec2ui& iChunk,
					   const float iLayerBias, std::vector<TileInstance>& oTiles) {
	oTiles.clear();
	const auto* chunk = iLayer.getChunk(iChunk);
	if (chunk == nullptr)
		return;
	const auto& tileset = *iAsset.tileset;
	const auto& size = iLayer.getSize();
	const uint32_t x0 = iChunk.x() * TilemapLayer::chunkSize;
	const uint32_t y0 = iChunk.y() * TilemapLayer::chunkSize;
	const uint32_t x1 = std::min(size.x(), x0 + TilemapLayer::chunkSize);
	const uint32_t y1 = std::min(size.y(), y0 + TilemapLayer::chunkSize);
	constexpr float overlayBias = RendererIsometric::tilemapLayerBias * 0.5f;
	const auto rectOf = [&tileset](const int32_t iTile) -> math::vec4 {
		const auto uv = tileset.getTileUv(static_cast<uint32_t>(iTile));
		return {uv[0].x(), uv[0].y(), uv[2].x(), uv[2].y()};
	};
	for (uint32_t y = y0; y < y1; ++y) {
		const uint32_t row = (y - y0) * TilemapLayer::chunkSize;
		for (uint32_t x = x0; x < x1; ++x) {
			const size_t cell = row + x - x0;
			const uint16_t id = chunk->terrain[cell];
			const uint16_t overlayId = chunk->overlay[cell];
			if (id == TilemapChunk::emptyId && overlayId == TilemapChunk::emptyId)
				continue;
			const uint8_t slope = iAsset.getCellSlope(x, y);
			const float baseZ = static_cast<float>(iAsset.getCellBaseHeight(x, y)) * scene::terrain::heightStep;
			const auto footX = static_cast<float>(x + 1);
			const auto footY = static_cast<float>(y + 1);
			if (id != TilemapChunk::emptyId) {
				const bool foundation = (chunk->flags[cell] & scene::terrain::foundationFlag) != 0;
				oTiles.push_back({.foot = {footX, footY, baseZ, iLayerBias},
								  .uvRect = rectOf(tileset.resolveTile(id, slope, foundation))});
			}
			if (overlayId != TilemapChunk::emptyId) {
				oTiles.push_back({.foot = {footX, footY, baseZ, iLayerBias + overlayBias},
								  .uvRect = rectOf(tileset.resolveTile(overlayId, slope))});
			}
		}
	}
}

void buildChunk(const scene::TilemapAsset& iAsset, const size_t iLayerIndex, const math::vec2ui& iChunk,
				ChunkGpu& ioChunk, const bool iLate) {
	auto& tiles = g_state->tileScratch;
	collectChunkTiles(iAsset, iAsset.layers[iLayerIndex], iChunk,
					  static_cast<float>(iLayerIndex) * RendererIsometric::tilemapLayerBias, tiles);
	ioChunk.generation = iAsset.layers[iLayerIndex].getChunkGeneration(iChunk);
	ioChunk.heightGeneration = iAsset.getChunkHeightGeneration(iChunk);
	ioChunk.instanceCount = static_cast<uint32_t>(tiles.size());
	++g_state->prepareStats.builtChunkCount;
	if (iLate)
		++g_state->prepareStats.lateBuiltChunkCount;
	if (tiles.empty()) {
		ioChunk.drawData.reset();
		ioChunk.capacity = 0;
		return;
	}
	if (!ioChunk.drawData || ioChunk.capacity < ioChunk.instanceCount) {
		ioChunk.capacity =
				std::min(g_maxChunkInstances, std::bit_ceil(std::max(ioChunk.instanceCount, g_minChunkCapacity)));
		ioChunk.drawData = makeChunkDraw(ioChunk.capacity);
	}
	const auto bytes = static_cast<uint32_t>(tiles.size() * sizeof(TileInstance));
	ioChunk.drawData->setInstanceData(tiles.data(), bytes);
	g_state->prepareStats.uploadedBytes += bytes;
}

void syncLayer(const scene::TilemapAsset& iAsset, const size_t iLayerIndex, LayerGpu& ioLayer, const bool iStale,
			   const bool iLate) {
	const auto& layer = iAsset.layers[iLayerIndex];
	const auto chunkGrid = layer.getChunkGridSize();
	if (ioLayer.size != layer.getSize() || ioLayer.chunkGrid != chunkGrid) {
		ioLayer.chunks.clear();
		ioLayer.chunks.resize(static_cast<size_t>(chunkGrid.x()) * chunkGrid.y());
		ioLayer.size = layer.getSize();
		ioLayer.chunkGrid = chunkGrid;
	}
	ioLayer.occupiedChunkCount = 0;
	for (uint32_t cy = 0; cy < chunkGrid.y(); ++cy) {
		for (uint32_t cx = 0; cx < chunkGrid.x(); ++cx) {
			auto& chunk = ioLayer.chunks[static_cast<size_t>(cy) * chunkGrid.x() + cx];
			if (iStale || chunk.generation != layer.getChunkGeneration({cx, cy}) ||
				chunk.heightGeneration != iAsset.getChunkHeightGeneration({cx, cy}))
				buildChunk(iAsset, iLayerIndex, {cx, cy}, chunk, iLate);
			if (chunk.instanceCount > 0)
				++ioLayer.occupiedChunkCount;
		}
	}
}

auto syncAsset(const scene::TilemapAsset& iAsset, const bool iLate) -> AssetGpu& {
	auto& entry = g_state->cache[&iAsset];
	entry.lastUsedPrepare = g_state->prepareIndex;
	if (entry.layers.size() != iAsset.layers.size())
		entry.layers.resize(iAsset.layers.size());
	const uint64_t signature = getTilesetSignature(*iAsset.tileset);
	const bool stale = entry.tileset != iAsset.tileset.get() || entry.tilesetSignature != signature;
	entry.tileset = iAsset.tileset.get();
	entry.tilesetSignature = signature;
	for (size_t i = 0; i < iAsset.layers.size(); ++i) syncLayer(iAsset, i, entry.layers[i], stale, iLate);
	return entry;
}

void drawTilemapChunks(const PendingTilemap& iPending, const AssetGpu& iEntry) {
	auto& state = *g_state;
	const auto& asset = *iPending.asset;
	const auto& tileset = *asset.tileset;
	const auto& camera = state.camera;
	const auto tileWidth = static_cast<float>(camera.getConfig().tileSize.x());
	const float pixelScale = tileWidth / static_cast<float>(std::max(1u, tileset.tileWidth));
	const float spriteHeight = static_cast<float>(tileset.tileHeight) * pixelScale;
	const float terrainRaise =
			static_cast<float>(asset.getMaxCornerHeight()) * scene::terrain::heightStep * camera.getConfig().zStep;
	state.scene.origin = math::vec4{iPending.origin.x(), iPending.origin.y(), iPending.origin.z(), 0.f};
	state.scene.tile = math::vec4{tileWidth, spriteHeight, state.flatSlope, 0.f};
	state.scene.ids = {iPending.entityId, gpu::Framebuffer::isLinearTargetBound() ? 1 : 0, 0, 0};
	state.sceneUbo->setData(&state.scene, sizeof(SceneUbo), 0);
	gpu::RenderCommand::bindTextures(std::span{&tileset.texture, 1});
	for (size_t index = 0; index < asset.layers.size(); ++index) {
		if (!asset.layers[index].visible)
			continue;
		const auto& layer = iEntry.layers[index];
		if (layer.occupiedChunkCount == 0)
			continue;
		for (uint32_t cy = 0; cy < layer.chunkGrid.y(); ++cy) {
			for (uint32_t cx = 0; cx < layer.chunkGrid.x(); ++cx) {
				const auto& chunk = layer.chunks[static_cast<size_t>(cy) * layer.chunkGrid.x() + cx];
				if (chunk.instanceCount == 0 || !chunk.drawData)
					continue;
				const math::vec2ui first{cx * TilemapLayer::chunkSize, cy * TilemapLayer::chunkSize};
				const math::vec2ui last{std::min(layer.size.x(), first.x() + TilemapLayer::chunkSize),
										std::min(layer.size.y(), first.y() + TilemapLayer::chunkSize)};
				if (isChunkOutside(camera, iPending.origin, first, last, spriteHeight + terrainRaise)) {
					++state.stats.culledChunkCount;
					continue;
				}
				gpu::RenderCommand::drawDataInstanced(chunk.drawData, /*iIndexCount=*/6u, chunk.instanceCount);
				++state.stats.drawCallCount;
				state.stats.opaqueCount += chunk.instanceCount;
			}
		}
	}
}

void drawTilePass() {
	auto& state = *g_state;
	if (state.tilemaps.empty())
		return;
	setPassParams(g_cutoutAlpha);
	for (const auto& pending: state.tilemaps) drawTilemapChunks(pending, syncAsset(*pending.asset, true));
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
	g_state->spriteDraw = makeSpriteDraw();
	g_state->whiteTexture =
			gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {1, 1}, .format = gpu::ImageFormat::Rgba8});
	uint32_t white = 0xffffffff;
	g_state->whiteTexture->setData(&white, sizeof(uint32_t));
	g_state->sceneUbo = gpu::UniformBuffer::create(sizeof(SceneUbo), 0, g_rendererKey);
	g_state->instances = gpu::StorageBuffer::create(maxBillboardsPerBatch * sizeof(GpuInstance), 2, g_rendererKey);
	g_state->spriteInstances = gpu::StorageBuffer::create(maxSpritesPerBatch * sizeof(GpuSprite), 2, g_rendererKey);
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
	const float zoom = iCamera.getZoom();
	state.snapZoom = zoom >= 1.f ? zoom : 0.f;
	const auto& viewport = iCamera.getViewportSize();
	state.snapOffset = {static_cast<float>(viewport.x() % 2u) * 0.5f, static_cast<float>(viewport.y() % 2u) * 0.5f};
	state.opaque.clear();
	state.translucent.clear();
	state.sprites.clear();
	state.tilemaps.clear();
	state.frameTextures.clear();
	state.lastTexture = nullptr;
	state.lastFrameTexture = g_whiteTexture;
}

void RendererIsometric::drawBillboard(const IsometricBillboard& iBillboard) {
	if (!isReady())
		return;
	queue(iBillboard);
}

void RendererIsometric::drawSprite(const shared<gpu::Texture2D>& iTexture, const IsometricSprite& iSprite) {
	drawSprites(iTexture, std::span{&iSprite, 1});
}

void RendererIsometric::drawSprites(const shared<gpu::Texture2D>& iTexture,
									const std::span<const IsometricSprite> iSprites) {
	if (!isReady() || iSprites.empty())
		return;
	auto& state = *g_state;
	const uint16_t frameTexture = frameTextureOf(iTexture);
	state.sprites.reserve(state.sprites.size() + iSprites.size());
	for (const auto& sprite: iSprites) queueSprite(frameTexture, sprite);
}

void RendererIsometric::drawObject(const IsometricObjectSet& iSet, const IsometricObjectDef& iObject,
								   const math::vec3& iOrigin, const math::vec4& iColor, const int iEntityId) {
	if (!isReady() || !iSet.getTexture())
		return;
	const auto& state = *g_state;
	const auto& texture = iSet.getTexture();
	const math::vec2ui textureSize = texture->getSize();
	if (textureSize.x() == 0 || textureSize.y() == 0)
		return;
	const auto sheetWidth = static_cast<float>(textureSize.x());
	const auto sheetHeight = static_cast<float>(textureSize.y());
	const float scale = static_cast<float>(state.camera.getConfig().tileSize.x()) / iSet.getTileWidth();
	const uint32_t color = packColor(iColor);
	const uint16_t frameTexture = frameTextureOf(texture);
	const float halfU = 0.5f / sheetWidth;
	const float halfV = 0.5f / sheetHeight;
	for (const auto& slice: iObject.slices) {
		const float rectWidth = slice.rect.z();
		const float rectHeight = slice.rect.w();
		queueSprite(frameTexture, {.foot = iOrigin + math::vec3{static_cast<float>(slice.cell.x() + 1),
																static_cast<float>(slice.cell.y() + 1), 0.f},
								   .size = {rectWidth * scale, rectHeight * scale},
								   .anchor = {slice.anchor.x() / rectWidth, 1.f - slice.anchor.y() / rectHeight},
								   .uvRect = {slice.rect.x() / sheetWidth + halfU,
											  1.f - (slice.rect.y() + rectHeight) / sheetHeight + halfV,
											  (slice.rect.x() + rectWidth) / sheetWidth - halfU,
											  1.f - slice.rect.y() / sheetHeight - halfV},
								   .color = color,
								   .depthBias = spriteBias,
								   .entityId = iEntityId});
	}
}

void RendererIsometric::drawTilemap(const scene::TilemapAsset& iAsset, const math::Transform& iWorldTransform,
									const int iEntityId) {
	if (!isReady() || !iAsset.tileset || !iAsset.tileset->texture || iAsset.layers.empty())
		return;
	g_state->tilemaps.push_back({.asset = &iAsset, .origin = iWorldTransform.translation(), .entityId = iEntityId});
}

void RendererIsometric::beginPrepare() {
	if (!isReady())
		return;
	++g_state->prepareIndex;
	g_state->prepareStats = PrepareStatistics{};
	std::erase_if(g_state->cache, [](const auto& iEntry) -> bool {
		return iEntry.second.lastUsedPrepare + cacheEvictionPrepares < g_state->prepareIndex;
	});
}

void RendererIsometric::prepareTilemap(const scene::TilemapAsset& iAsset) {
	OWL_PROFILE_FUNCTION()

	if (!isReady() || iAsset.layers.empty() || !iAsset.tileset)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	syncAsset(iAsset, false);
}

void RendererIsometric::clearCache() {
	if (g_state)
		g_state->cache.clear();
}

auto RendererIsometric::getCachedAssetCount() -> size_t { return g_state ? g_state->cache.size() : 0; }

auto RendererIsometric::getPrepareStatistics() -> PrepareStatistics {
	return g_state ? g_state->prepareStats : PrepareStatistics{};
}

void RendererIsometric::endScene() {
	OWL_PROFILE_FUNCTION()

	if (!isReady())
		return;
	auto& state = *g_state;
	if (state.opaque.empty() && state.translucent.empty() && state.sprites.empty() && state.tilemaps.empty())
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	gpu::RenderCommand::beginBatch();
	drawTilePass();
	drawOpaquePass();
	drawSpritePass();
	drawTranslucentPass();
	gpu::RenderCommand::endBatch();
	state.opaque.clear();
	state.translucent.clear();
	state.sprites.clear();
	state.tilemaps.clear();
}

auto RendererIsometric::getStatistics() -> Statistics { return g_state ? g_state->stats : Statistics{}; }

void RendererIsometric::resetStats() {
	if (g_state)
		g_state->stats = Statistics{};
}

}// namespace owl::renderer
