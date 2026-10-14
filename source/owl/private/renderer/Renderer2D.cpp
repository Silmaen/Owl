/**
 * @file Renderer2D.cpp
 * @author Silmaen
 * @date 18/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer2D.h"

#include "app/Application.h"
#include "renderer/BackgroundRenderer.h"
#include "renderer/RendererTilemap.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace owl::renderer {

namespace utils {

namespace {
constexpr uint32_t g_maxQuadsPerBatch = 20000;
constexpr uint32_t g_maxCirclesPerBatch = 20000;
constexpr uint32_t g_maxLinesPerBatch = 20000;
constexpr uint32_t g_maxTextGlyphsPerBatch = 20000;
constexpr uint32_t g_maxTransientWorldsPerBatch = 20000;

constexpr std::array g_quadVertexPositions = {math::vec4{-0.5f, -0.5f, 0.0f, 1.0f}, math::vec4{0.5f, -0.5f, 0.0f, 1.0f},
											  math::vec4{0.5f, 0.5f, 0.0f, 1.0f}, math::vec4{-0.5f, 0.5f, 0.0f, 1.0f}};

uint32_t g_MaxTextureSlots = 0;

struct QuadInstance {
	int32_t worldIndex;
	uint32_t _pad0[3];
	math::vec4 color;
	std::array<math::vec2, 4> uv;
	uint32_t texIndex;
	int32_t entityId;
	math::vec2 tilingFactor;
};
static_assert(sizeof(QuadInstance) == 80, "QuadInstance must match shader std430 layout");

struct CircleInstance {
	int32_t worldIndex;
	uint32_t _pad0[3];
	math::vec4 color;
	float thickness;
	float fade;
	int32_t entityId;
	uint32_t _pad1;
};
static_assert(sizeof(CircleInstance) == 48, "CircleInstance must match shader std430 layout");

struct LineInstance {
	math::vec3 point1;
	float _pad0;
	math::vec3 point2;
	float _pad1;
	math::vec4 color;
	int32_t entityId;
	float _pad2[3];
};
static_assert(sizeof(LineInstance) == 64, "LineInstance must match shader std430 layout");

struct TextInstance {
	int32_t worldIndex;
	uint32_t _pad0[3];
	math::vec4 color;
	std::array<math::vec2, 4> uv;
	uint32_t texIndex;
	int32_t entityId;
	uint32_t _pad1[2];
};
static_assert(sizeof(TextInstance) == 80, "TextInstance must match shader std430 layout");

template<typename InstanceT>
struct BatchData {
	std::vector<InstanceT> instances;
	shared<gpu::DrawData> drawData;
	shared<gpu::StorageBuffer> ssbo;
};

template<typename InstanceT>
void resetBatch(BatchData<InstanceT>& iData, uint32_t iReserve) {
	iData.instances.clear();
	iData.instances.reserve(iReserve);
}

// Slot of a texture bound in the current batch, keyed by its backend identity; stale when generation differs.
struct TextureSlotEntry {
	uint64_t backendId = 0;
	uint32_t slot = 0;
	uint32_t generation = 0;
};

struct InternalData {
	struct CameraData {
		math::mat4 viewProjection;
	};
	CameraData cameraBuffer{};
	BatchData<QuadInstance> quad;
	BatchData<CircleInstance> circle;
	BatchData<LineInstance> line;
	BatchData<TextInstance> text;
	Renderer2D::Statistics stats;
	shared<gpu::Texture2D> whiteTexture;
	shared<gpu::UniformBuffer> cameraUniformBuffer;
	std::vector<shared<gpu::Texture2D>> textureSlots;
	uint32_t textureSlotIndex = 1;
	// Open-addressing table (power of two, at least twice the slot count), emptied by bumping the generation.
	std::vector<TextureSlotEntry> textureLookup;
	uint32_t textureLookupGeneration = 1;
	const gpu::Texture* lastTexture = nullptr;
	uint32_t lastTextureSlot = 0;
	std::vector<math::mat4> sceneWorlds;
	bool sceneWorldsDirty = false;
	uint32_t sceneWorldsCapacity = 0;
	shared<gpu::StorageBuffer> sceneWorldsSsbo;
	shared<gpu::StorageBuffer> sceneWorldsFallback;
	std::vector<math::mat4> transientWorlds;
	shared<gpu::StorageBuffer> transientWorldsSsbo;
};
}// namespace
}// namespace utils

namespace {
shared<utils::InternalData> g_Data;

// Identifier of this renderer's descriptor block, resolved once.
auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId("Renderer2D");
	return sId;
}

auto allocateTransientWorld(const math::mat4& iMatrix) -> int32_t {
	if (g_Data->transientWorlds.size() >= utils::g_maxTransientWorldsPerBatch)
		Renderer2D::nextBatch();
	const auto slot = static_cast<int32_t>(g_Data->transientWorlds.size());
	g_Data->transientWorlds.push_back(iMatrix);
	return -(slot + 1);
}

// Transient path of resolveWorldIndex, kept out of line so the scene-world path stays a compare.
[[gnu::noinline]] auto allocateTransientTransform(const math::Transform& iTransform) -> int32_t {
	return allocateTransientWorld(iTransform());
}

inline auto resolveWorldIndex(const int32_t iRequested, const math::Transform& iTransform) -> int32_t {
	return iRequested >= 0 ? iRequested : allocateTransientTransform(iTransform);
}

void forgetTextureSlots() {
	g_Data->lastTexture = nullptr;
	if (++g_Data->textureLookupGeneration != 0)
		return;
	std::ranges::fill(g_Data->textureLookup, utils::TextureSlotEntry{});
	g_Data->textureLookupGeneration = 1;
}

auto textureLookupEntry(const uint64_t iBackendId) -> utils::TextureSlotEntry& {
	const auto mask = g_Data->textureLookup.size() - 1;
	// Fibonacci hashing spreads the small consecutive ids of the backends over the table.
	auto index = static_cast<size_t>((iBackendId * 0x9E3779B97F4A7C15ULL) >> 32U) & mask;
	while (true) {
		auto& entry = g_Data->textureLookup[index];
		if (entry.generation != g_Data->textureLookupGeneration || entry.backendId == iBackendId)
			return entry;
		index = (index + 1) & mask;
	}
}

// Binds a texture not yet in the batch to a new slot (or starts a new batch), keyed by its backend identity.
[[gnu::noinline]] auto bindNewTextureSlot(const shared<gpu::Texture>& iTexture) -> uint32_t {
	const uint64_t backendId = iTexture->getBackendId();
	auto* entry = &textureLookupEntry(backendId);
	if (entry->generation != g_Data->textureLookupGeneration) {
		if (g_Data->textureSlotIndex >= utils::g_MaxTextureSlots) {
			Renderer2D::nextBatch();
			entry = &textureLookupEntry(backendId);
		}
		const uint32_t index = g_Data->textureSlotIndex++;
		g_Data->textureSlots[index] = std::static_pointer_cast<gpu::Texture2D>(iTexture);
		*entry = {.backendId = backendId, .slot = index, .generation = g_Data->textureLookupGeneration};
	}
	g_Data->lastTexture = iTexture.get();
	g_Data->lastTextureSlot = entry->slot;
	return entry->slot;
}

// Texture slot of a quad's texture; the few bound slots are scanned before the hash is consulted.
inline auto textureSlotOf(const shared<gpu::Texture>& iTexture) -> uint32_t {
	if (iTexture.get() == g_Data->lastTexture)
		return g_Data->lastTextureSlot;
	const auto* slots = g_Data->textureSlots.data();
	const uint32_t count = g_Data->textureSlotIndex;
	for (uint32_t i = 1; i < count; ++i)
		if (slots[i].get() == iTexture.get())
			return i;
	return bindNewTextureSlot(iTexture);
}
}// namespace

void Renderer2D::init() {
	OWL_PROFILE_FUNCTION()

	if (g_Data) {
		OWL_CORE_WARN("Renderer2D already initiated.")
		g_Data.reset();
	}
	g_Data = mkShared<utils::InternalData>();

	{
		const std::array<gpu::BindingDecl, 5> r2dBindings{
				gpu::BindingDecl{.binding = 0,
								 .type = gpu::BindingType::UniformBuffer,
								 .count = 1,
								 .stages = gpu::ShaderStage::Vertex},
				gpu::BindingDecl{.binding = 1,
								 .type = gpu::BindingType::CombinedImageSampler,
								 .count = 32,
								 .stages = gpu::ShaderStage::Fragment},
				gpu::BindingDecl{.binding = 2,
								 .type = gpu::BindingType::StorageBuffer,
								 .count = 1,
								 .stages = gpu::ShaderStage::Vertex},
				gpu::BindingDecl{.binding = 3,
								 .type = gpu::BindingType::StorageBuffer,
								 .count = 1,
								 .stages = gpu::ShaderStage::Vertex},
				gpu::BindingDecl{.binding = 4,
								 .type = gpu::BindingType::StorageBuffer,
								 .count = 1,
								 .stages = gpu::ShaderStage::Vertex},
		};
		gpu::RendererDescriptors::declare("Renderer2D", r2dBindings);
	}
	const gpu::RendererDescriptors::ScopedActive r2dScoped{blockId()};

	std::vector<uint32_t> quadIndices = {0, 1, 2, 2, 3, 0};
	std::vector<uint32_t> lineIndices = {0, 1};

	const auto setupCornerVbo = [&quadIndices](const shared<gpu::DrawData>& iDraw,
											   const std::string& iShaderName) -> void {
		iDraw->init({{"i_CornerIndex", gpu::ShaderDataType::Int}}, "renderer2D", quadIndices, iShaderName, {});
		constexpr std::array<int32_t, 4> corners{0, 1, 2, 3};
		iDraw->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(int32_t)));
	};

	g_Data->quad.drawData = gpu::DrawData::create();
	setupCornerVbo(g_Data->quad.drawData, "quad");

	g_Data->circle.drawData = gpu::DrawData::create();
	setupCornerVbo(g_Data->circle.drawData, "circle");

	g_Data->text.drawData = gpu::DrawData::create();
	setupCornerVbo(g_Data->text.drawData, "text");

	g_Data->line.drawData = gpu::DrawData::create();
	g_Data->line.drawData->init({{"i_EndpointIndex", gpu::ShaderDataType::Int}}, "renderer2D", lineIndices, "line",
								{.topology = gpu::PrimitiveTopology::Lines});
	{
		constexpr std::array<int32_t, 2> endpoints{0, 1};
		g_Data->line.drawData->setVertexData(endpoints.data(),
											 static_cast<uint32_t>(endpoints.size() * sizeof(int32_t)));
	}

	g_Data->whiteTexture =
			gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {1, 1}, .format = gpu::ImageFormat::Rgba8});
	uint32_t whiteTextureData = 0xffffffff;
	g_Data->whiteTexture->setData(&whiteTextureData, sizeof(uint32_t));

	utils::g_MaxTextureSlots = gpu::RenderCommand::getMaxTextureSlots();
	g_Data->textureSlots.resize(utils::g_MaxTextureSlots);
	g_Data->textureSlots[0] = g_Data->whiteTexture;
	g_Data->textureLookup.resize(std::bit_ceil(2 * size_t{utils::g_MaxTextureSlots}));

	g_Data->cameraUniformBuffer = gpu::UniformBuffer::create(sizeof(utils::InternalData::CameraData), 0, "Renderer2D");
	g_Data->cameraUniformBuffer->bind();

	g_Data->quad.ssbo =
			gpu::StorageBuffer::create(utils::g_maxQuadsPerBatch * sizeof(utils::QuadInstance), 2, "Renderer2D");
	g_Data->circle.ssbo =
			gpu::StorageBuffer::create(utils::g_maxCirclesPerBatch * sizeof(utils::CircleInstance), 2, "Renderer2D");
	g_Data->line.ssbo =
			gpu::StorageBuffer::create(utils::g_maxLinesPerBatch * sizeof(utils::LineInstance), 2, "Renderer2D");
	g_Data->text.ssbo =
			gpu::StorageBuffer::create(utils::g_maxTextGlyphsPerBatch * sizeof(utils::TextInstance), 2, "Renderer2D");

	g_Data->sceneWorldsFallback =
			gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(math::mat4)), 3, "Renderer2D");
	{
		const math::mat4 identity = math::identity<float, 4>();
		g_Data->sceneWorldsFallback->setData(&identity, static_cast<uint32_t>(sizeof(math::mat4)), 0);
	}
	g_Data->transientWorldsSsbo =
			gpu::StorageBuffer::create(utils::g_maxTransientWorldsPerBatch * sizeof(math::mat4), 4, "Renderer2D");
	g_Data->transientWorlds.reserve(utils::g_maxTransientWorldsPerBatch);
}

void Renderer2D::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_Data) {
		OWL_CORE_WARN("Renderer2D already shutdown.")
		return;
	}

	g_Data->cameraUniformBuffer.reset();
	g_Data->whiteTexture.reset();
	for (auto& text: g_Data->textureSlots) {
		if (text == nullptr)
			continue;
		text.reset();
	}
	g_Data->quad = utils::BatchData<utils::QuadInstance>{};
	g_Data->circle = utils::BatchData<utils::CircleInstance>{};
	g_Data->line = utils::BatchData<utils::LineInstance>{};
	g_Data->text = utils::BatchData<utils::TextInstance>{};
	g_Data->sceneWorlds.clear();
	g_Data->sceneWorldsSsbo.reset();
	g_Data->sceneWorldsFallback.reset();
	g_Data->transientWorldsSsbo.reset();
	g_Data->transientWorlds.clear();
	g_Data->transientWorlds.shrink_to_fit();
	g_Data.reset();
	gpu::RendererDescriptors::release("Renderer2D");
}

void Renderer2D::beginScene(const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	{
		const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
		g_Data->cameraBuffer.viewProjection = iCamera.getViewProjection();
		g_Data->cameraUniformBuffer->setData(&g_Data->cameraBuffer, sizeof(utils::InternalData::CameraData), 0);
	}
	startBatch();
	RendererTilemap::beginScene(iCamera);
}

void Renderer2D::endScene() {
	OWL_PROFILE_FUNCTION()

	flush();
}

void Renderer2D::flush() {
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	float bgTexIndex = 0.0f;
	if (BackgroundRenderer::hasPending()) {
		if (const auto bgTex = BackgroundRenderer::getPendingTexture()) {
			bgTexIndex = static_cast<float>(g_Data->textureSlotIndex);
			g_Data->textureSlots[g_Data->textureSlotIndex] = bgTex;
			g_Data->textureSlotIndex++;
		}
	}

	shared<gpu::StorageBuffer> retiredSceneWorlds = g_Data->sceneWorldsDirty ? reserveSceneWorlds() : nullptr;
	gpu::RenderCommand::beginBatch();
	retiredSceneWorlds.reset();
	gpu::RenderCommand::bindTextures(std::span{g_Data->textureSlots}.first(g_Data->textureSlotIndex));

	if (g_Data->sceneWorldsDirty) {
		g_Data->sceneWorldsDirty = false;
		if (!g_Data->sceneWorlds.empty() && g_Data->sceneWorldsSsbo != nullptr)
			g_Data->sceneWorldsSsbo->setData(g_Data->sceneWorlds.data(),
											 static_cast<uint32_t>(g_Data->sceneWorlds.size() * sizeof(math::mat4)), 0);
	}
	if (!g_Data->transientWorlds.empty()) {
		g_Data->transientWorldsSsbo->setData(g_Data->transientWorlds.data(),
											 static_cast<uint32_t>(g_Data->transientWorlds.size() * sizeof(math::mat4)),
											 0);
	}
	g_Data->transientWorldsSsbo->bind(/*iBinding=*/4u);

	const bool hasSceneWorlds = !g_Data->sceneWorlds.empty() && g_Data->sceneWorldsSsbo != nullptr;
	(hasSceneWorlds ? g_Data->sceneWorldsSsbo : g_Data->sceneWorldsFallback)->bind(/*iBinding=*/3u);

	BackgroundRenderer::flushPending(bgTexIndex);

	RendererTilemap::flushPending();

	if (!g_Data->quad.instances.empty()) {
		const auto count = static_cast<uint32_t>(g_Data->quad.instances.size());
		g_Data->quad.ssbo->setData(g_Data->quad.instances.data(),
								   static_cast<uint32_t>(count * sizeof(utils::QuadInstance)), 0);
		g_Data->quad.ssbo->bind();
		gpu::RenderCommand::drawDataInstanced(g_Data->quad.drawData, /*iIndexCount=*/6u, count);
		g_Data->stats.drawCalls++;
	}
	if (!g_Data->circle.instances.empty()) {
		const auto count = static_cast<uint32_t>(g_Data->circle.instances.size());
		g_Data->circle.ssbo->setData(g_Data->circle.instances.data(),
									 static_cast<uint32_t>(count * sizeof(utils::CircleInstance)), 0);
		g_Data->circle.ssbo->bind();
		gpu::RenderCommand::drawDataInstanced(g_Data->circle.drawData, /*iIndexCount=*/6u, count);
		g_Data->stats.drawCalls++;
	}
	if (!g_Data->line.instances.empty()) {
		const auto count = static_cast<uint32_t>(g_Data->line.instances.size());
		g_Data->line.ssbo->setData(g_Data->line.instances.data(),
								   static_cast<uint32_t>(count * sizeof(utils::LineInstance)), 0);
		g_Data->line.ssbo->bind();
		gpu::RenderCommand::drawDataInstanced(g_Data->line.drawData, /*iIndexCount=*/2u, count);
		g_Data->stats.drawCalls++;
	}
	if (!g_Data->text.instances.empty()) {
		const auto count = static_cast<uint32_t>(g_Data->text.instances.size());
		g_Data->text.ssbo->setData(g_Data->text.instances.data(),
								   static_cast<uint32_t>(count * sizeof(utils::TextInstance)), 0);
		g_Data->text.ssbo->bind();
		gpu::RenderCommand::drawDataInstanced(g_Data->text.drawData, /*iIndexCount=*/6u, count);
		g_Data->stats.drawCalls++;
	}
	gpu::RenderCommand::endBatch();
}

void Renderer2D::startBatch() {
	utils::resetBatch(g_Data->quad, utils::g_maxQuadsPerBatch);
	utils::resetBatch(g_Data->circle, utils::g_maxCirclesPerBatch);
	utils::resetBatch(g_Data->line, utils::g_maxLinesPerBatch);
	utils::resetBatch(g_Data->text, utils::g_maxTextGlyphsPerBatch);
	g_Data->textureSlotIndex = 1;
	forgetTextureSlots();
	g_Data->transientWorlds.clear();
}

void Renderer2D::setSceneWorlds(const std::span<const math::mat4> iWorlds) {
	if (!g_Data)
		return;
	g_Data->sceneWorlds.assign(iWorlds.begin(), iWorlds.end());
	g_Data->sceneWorldsDirty = true;
}

auto Renderer2D::reserveSceneWorlds() -> shared<gpu::StorageBuffer> {
	const auto count = static_cast<uint32_t>(g_Data->sceneWorlds.size());
	if (count == 0 || (count <= g_Data->sceneWorldsCapacity && g_Data->sceneWorldsSsbo != nullptr))
		return nullptr;
	g_Data->sceneWorldsCapacity = std::bit_ceil(count);
	auto retired = std::move(g_Data->sceneWorldsSsbo);
	g_Data->sceneWorldsSsbo = gpu::StorageBuffer::create(
			g_Data->sceneWorldsCapacity * static_cast<uint32_t>(sizeof(math::mat4)), 3, "Renderer2D");
	return retired;
}

void Renderer2D::nextBatch() {
	flush();
	startBatch();
}

void Renderer2D::drawLine(const LineData& iLineData) {
	if (g_Data->line.instances.size() >= utils::g_maxLinesPerBatch)
		nextBatch();
	g_Data->line.instances.push_back(utils::LineInstance{.point1 = iLineData.point1,
														 ._pad0 = 0.f,
														 .point2 = iLineData.point2,
														 ._pad1 = 0.f,
														 .color = iLineData.color,
														 .entityId = iLineData.entityId,
														 ._pad2 = {0.f, 0.f, 0.f}});
	g_Data->stats.lineCount++;
}

void Renderer2D::drawRect(const RectData& iRectData) {
	const math::mat4 trans = iRectData.transform();
	std::array<math::vec3, utils::g_quadVertexPositions.size()> corners;
	for (size_t i = 0; i < corners.size(); ++i) corners[i] = math::vec3{trans * utils::g_quadVertexPositions[i]};
	for (size_t i = 0; i < corners.size(); ++i)
		drawLine({.point1 = corners[i],
				  .point2 = corners[(i + 1) % corners.size()],
				  .color = iRectData.color,
				  .entityId = iRectData.entityId});
}

void Renderer2D::drawPolyLine(const PolyLineData& iLineData) {
	if (iLineData.points.size() < 2) {
		OWL_CORE_WARN("Too few points in the multiline with ID {}.", iLineData.entityId)
		return;
	}
	const math::mat4 trans = iLineData.transform();
	const auto toWorld = [&trans](const math::vec3& iPoint) -> math::vec3 {
		return math::vec3{trans * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f}};
	};
	const math::vec3 first = toWorld(iLineData.points.front());
	math::vec3 previous = first;
	for (size_t i = 1; i < iLineData.points.size(); ++i) {
		const math::vec3 current = toWorld(iLineData.points[i]);
		drawLine({.point1 = previous, .point2 = current, .color = iLineData.color, .entityId = iLineData.entityId});
		previous = current;
	}
	if (iLineData.closed)
		drawLine({.point1 = previous, .point2 = first, .color = iLineData.color, .entityId = iLineData.entityId});
}

void Renderer2D::drawCircle(const CircleData& iCircleData) {
	OWL_PROFILE_FUNCTION()

	if (g_Data->circle.instances.size() >= utils::g_maxCirclesPerBatch)
		nextBatch();
	const int32_t worldIndex = resolveWorldIndex(iCircleData.worldIndex, iCircleData.transform);
	g_Data->circle.instances.push_back(utils::CircleInstance{.worldIndex = worldIndex,
															 ._pad0 = {0u, 0u, 0u},
															 .color = iCircleData.color,
															 .thickness = iCircleData.thickness,
															 .fade = iCircleData.fade,
															 .entityId = iCircleData.entityId,
															 ._pad1 = 0u});
	g_Data->stats.quadCount++;
}

void Renderer2D::drawQuad(const Quad2DData& iQuadData) {
	OWL_PROFILE_FUNCTION()

	if (g_Data->quad.instances.size() >= utils::g_maxQuadsPerBatch)
		nextBatch();
	const uint32_t textureIndex = iQuadData.texture == nullptr ? 0 : textureSlotOf(iQuadData.texture);
	const int32_t worldIndex = resolveWorldIndex(iQuadData.worldIndex, iQuadData.transform);
	g_Data->quad.instances.push_back(utils::QuadInstance{.worldIndex = worldIndex,
														 ._pad0 = {0u, 0u, 0u},
														 .color = iQuadData.color,
														 .uv = iQuadData.textureCoords,
														 .texIndex = textureIndex,
														 .entityId = iQuadData.entityId,
														 .tilingFactor = iQuadData.tilingFactor});
	g_Data->stats.quadCount++;
}

void Renderer2D::drawString(const StringData& iStringData) {
	if (iStringData.font == nullptr) {
		OWL_CORE_ERROR("Renderer2D::drawString: Font not set.")
		return;
	}

	const auto& font = *iStringData.font;
	const uint32_t textureIndex = textureSlotOf(font.getAtlasTexture());
	const math::mat4 stringMat = iStringData.transform();
	const auto emitGlyph = [&](const data::fonts::Font::GlyphMetrics& iGlyph) -> void {
		const auto& [quad, uv] = iGlyph;
		if (g_Data->text.instances.size() >= utils::g_maxTextGlyphsPerBatch)
			nextBatch();

		const math::vec2 glyphCenter = (quad.min() + quad.max()) * 0.5f;
		const math::vec2 glyphSize = quad.max() - quad.min();
		math::Transform glyphLocal;
		glyphLocal.translation() = math::vec3{glyphCenter.x(), glyphCenter.y(), 0.f};
		glyphLocal.scale() = math::vec3{glyphSize.x(), glyphSize.y(), 1.f};
		const math::mat4 glyphMat = stringMat * glyphLocal();

		const std::array<math::vec2, 4> glyphUv{
				math::vec2{uv.min().x(), uv.min().y()}, math::vec2{uv.max().x(), uv.min().y()},
				math::vec2{uv.max().x(), uv.max().y()}, math::vec2{uv.min().x(), uv.max().y()}};
		const int32_t glyphWorldIndex = allocateTransientWorld(glyphMat);
		g_Data->text.instances.push_back(utils::TextInstance{.worldIndex = glyphWorldIndex,
															 ._pad0 = {0u, 0u, 0u},
															 .color = iStringData.color,
															 .uv = glyphUv,
															 .texIndex = textureIndex,
															 .entityId = iStringData.entityId,
															 ._pad1 = {0u, 0u}});
		g_Data->stats.quadCount++;
	};
	data::fonts::forEachLaidOutGlyph(font, iStringData.text, iStringData.layout, emitGlyph);
}

void Renderer2D::resetStats() {
	g_Data->stats.drawCalls = 0;
	g_Data->stats.quadCount = 0;
	g_Data->stats.lineCount = 0;
}

auto Renderer2D::getStats() -> Statistics { return g_Data->stats; }

}// namespace owl::renderer
