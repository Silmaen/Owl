/**
 * @file RendererParticles.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererParticles.h"

#include "renderer/Renderer3D.h"
#include "renderer/gpu/ComputeShader.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace owl::renderer {

struct ParticleGpuState {
	// Particle slots, written by the simulation only.
	shared<gpu::StorageBuffer> particles;
	// Trail points, written by the simulation only.
	shared<gpu::StorageBuffer> trails;
	// Sort items of an alpha-blended emitter.
	shared<gpu::StorageBuffer> order;
	// The step of the frame.
	shared<gpu::StorageBuffer> step;
	// The camera of the sort.
	shared<gpu::StorageBuffer> sortParams;
	// Capacity the buffers were made for.
	uint32_t capacity = 0;
	// Trail length the buffers were made for.
	uint32_t trailLength = 0;
};

namespace {

constexpr auto g_rendererKey = "particles";
constexpr uint32_t g_sortSize = g_maxSortedParticles;
constexpr uint32_t g_groupSize = 64;

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

struct DrawUbo {
	math::mat4 viewProjection = math::identity<float, 4>();
	math::vec4 right{1.f, 0.f, 0.f, 0.f};
	math::vec4 up{0.f, 1.f, 0.f, 0.f};
	math::vec4 eye{0.f, 0.f, 0.f, 0.f};
	math::vec4 forward{0.f, 0.f, -1.f, 0.f};
	std::array<math::vec4, 4> emitter{};
	std::array<math::vec4, g_particleTableSize> colors{};
	std::array<math::vec4, g_particleTableSize / 4> sizes{};
	std::array<uint32_t, 4> config{0, 0, 0, 0};
	std::array<uint32_t, 4> trail{0, 0, 0, 0};
	math::vec4 trailParams{0.f, 0.f, 0.f, 0.f};
};

struct SortParams {
	math::vec4 eye{0.f, 0.f, 0.f, 0.f};
	math::vec4 forward{0.f, 0.f, -1.f, 0.f};
	std::array<math::vec4, 4> emitter{};
	std::array<uint32_t, 4> config{0, 0, 0, 0};
};

struct SortItem {
	float key = 0.f;
	uint32_t value = 0;
};

struct InternalData {
	shared<gpu::UniformBuffer> uniforms;
	std::array<shared<gpu::DrawData>, 2> particleDraws;
	std::array<shared<gpu::DrawData>, 2> trailDraws;
	shared<gpu::ComputeShader> simulate;
	shared<gpu::ComputeShader> sortKeys;
	shared<gpu::ComputeShader> sort;
	shared<gpu::StorageBuffer> dummyParticles;
	shared<gpu::StorageBuffer> dummyOrder;
	shared<gpu::StorageBuffer> entityTrail;
	shared<gpu::Texture2D> white;
	DrawUbo ubo;
	RendererParticles::Stats stats;
	bool sceneOpen = false;
};

shared<InternalData> g_data;

auto columnsOf(const math::mat4& iMatrix) -> std::array<math::vec4, 4> {
	std::array<math::vec4, 4> columns{};
	for (size_t col = 0; col < 4; ++col)
		columns[col] = math::vec4{iMatrix(0, col), iMatrix(1, col), iMatrix(2, col), iMatrix(3, col)};
	return columns;
}

auto drawIndexOf(const ParticleBlend iBlend) -> size_t { return iBlend == ParticleBlend::Alpha ? 1 : 0; }

auto stateOf(const ParticleBlend iBlend) -> gpu::PipelineState {
	return {.blendMode = iBlend == ParticleBlend::Alpha ? gpu::BlendMode::Alpha : gpu::BlendMode::Additive,
			.depthTest = true,
			.depthWrite = false};
}

void fillTables(DrawUbo& ioUbo, const ParticleTables& iTables) {
	ioUbo.colors = iTables.color;
	for (uint32_t index = 0; index < g_particleTableSize; ++index)
		ioUbo.sizes[index / 4][index % 4] = iTables.size[index];
}

auto ensureGpuState(ParticleSystem& ioSystem) -> ParticleGpuState* {
	const uint32_t capacity = ioSystem.getCapacity();
	const uint32_t trail = ioSystem.getTrailLength();
	if (capacity == 0)
		return nullptr;
	if (const auto& state = ioSystem.getGpuState(); state && state->capacity == capacity && state->trailLength == trail)
		return state.get();
	auto state = mkShared<ParticleGpuState>();
	state->capacity = capacity;
	state->trailLength = trail;
	state->particles =
			gpu::StorageBuffer::create(capacity * static_cast<uint32_t>(sizeof(GpuParticle)), 1, g_rendererKey);
	state->trails = gpu::StorageBuffer::create(
			std::max(capacity * trail, 1u) * static_cast<uint32_t>(sizeof(math::vec4)), 2, g_rendererKey);
	state->order = gpu::StorageBuffer::create(g_sortSize * static_cast<uint32_t>(sizeof(SortItem)), 2, g_rendererKey);
	state->step = gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(ParticleStepParams)), 0, g_rendererKey);
	state->sortParams = gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(SortParams)), 0, g_rendererKey);
	if (!state->particles || !state->trails || !state->order || !state->step || !state->sortParams) {
		OWL_CORE_ERROR("RendererParticles: Failed to create the buffers of a system of {} particles.", capacity)
		return nullptr;
	}
	ioSystem.setGpuState(state);
	return state.get();
}

auto makeDraw(const char* iShader, const std::vector<int32_t>& iVertices, std::vector<uint32_t> iIndices,
			  const ParticleBlend iBlend) -> shared<gpu::DrawData> {
	auto draw = gpu::DrawData::create();
	draw->init({{"i_Index", gpu::ShaderDataType::Int}}, g_rendererKey, iIndices, iShader, stateOf(iBlend));
	draw->setVertexData(iVertices.data(), static_cast<uint32_t>(iVertices.size() * sizeof(int32_t)));
	return draw;
}

}// namespace

void RendererParticles::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data)
		shutdown();
	if (gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Null)
		return;
	g_data = mkShared<InternalData>();
	const std::array<gpu::BindingDecl, 5> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::VertexFragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = 1,
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
	gpu::RendererDescriptors::declare(g_rendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	g_data->uniforms = gpu::UniformBuffer::create(static_cast<uint32_t>(sizeof(DrawUbo)), 0, g_rendererKey);
	const std::vector<int32_t> corners{0, 1, 2, 3};
	std::vector<int32_t> ribbon(size_t{2} * (g_maxTrailPoints + 1));
	for (size_t index = 0; index < ribbon.size(); ++index) ribbon[index] = static_cast<int32_t>(index);
	std::vector<uint32_t> ribbonIndices;
	ribbonIndices.reserve(size_t{6} * g_maxTrailPoints);
	for (uint32_t segment = 0; segment < g_maxTrailPoints; ++segment) {
		const uint32_t first = 2 * segment;
		for (const uint32_t offset: {0u, 1u, 2u, 2u, 1u, 3u}) ribbonIndices.push_back(first + offset);
	}
	for (const auto blend: {ParticleBlend::Additive, ParticleBlend::Alpha}) {
		g_data->particleDraws[drawIndexOf(blend)] = makeDraw("particle", corners, {0, 1, 2, 2, 3, 0}, blend);
		g_data->trailDraws[drawIndexOf(blend)] = makeDraw("particle_trail", ribbon, ribbonIndices, blend);
	}
	g_data->dummyParticles = gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(GpuParticle)), 2, g_rendererKey);
	g_data->dummyOrder = gpu::StorageBuffer::create(static_cast<uint32_t>(sizeof(SortItem)), 3, g_rendererKey);
	g_data->entityTrail =
			gpu::StorageBuffer::create(g_maxTrailPoints * static_cast<uint32_t>(sizeof(math::vec4)), 4, g_rendererKey);
	g_data->white =
			gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {1, 1}, .format = gpu::ImageFormat::Rgba8});
	uint32_t whiteData = 0xffffffff;
	g_data->white->setData(&whiteData, sizeof(uint32_t));
	g_data->simulate = gpu::ComputeShader::create("particle_simulate", g_rendererKey);
	g_data->sortKeys = gpu::ComputeShader::create("particle_sort_keys", g_rendererKey);
	g_data->sort = gpu::ComputeShader::create("bitonic_sort", "bitonic_sort");
	if (!g_data->simulate || !g_data->sortKeys || !g_data->sort)
		OWL_CORE_ERROR("RendererParticles: Failed to create the compute shaders, the particles are not simulated.")
}

void RendererParticles::shutdown() {
	if (!g_data)
		return;
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

auto RendererParticles::isReady() -> bool { return g_data != nullptr; }

auto RendererParticles::createSystem() -> shared<ParticleSystem> {
	return mkShared<ParticleSystem>(g_data && g_data->simulate ? ParticleSystem::Path::Gpu : ParticleSystem::Path::Cpu);
}

void RendererParticles::prepare(ParticleSystem& ioSystem, const ParticleEmitterSettings& iSettings,
								const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !g_data->simulate || ioSystem.getPath() != ParticleSystem::Path::Gpu)
		return;
	auto* const state = ensureGpuState(ioSystem);
	if (state == nullptr)
		return;
	if (ioSystem.hasPendingStep()) {
		const auto& step = ioSystem.takeStep();
		state->step->setData(&step, static_cast<uint32_t>(sizeof(ParticleStepParams)), 0);
		g_data->simulate->bindStorageBuffer(0, state->step);
		g_data->simulate->bindStorageBuffer(1, state->particles);
		g_data->simulate->bindStorageBuffer(2, state->trails);
		g_data->simulate->dispatch((state->capacity + g_groupSize - 1) / g_groupSize, 1, 1);
		gpu::RenderCommand::storageBufferMemoryBarrier();
		++g_data->stats.steps;
	}
	if (iSettings.blend != ParticleBlend::Alpha || !g_data->sortKeys || !g_data->sort)
		return;
	const math::mat4 toWorld = inverse(iCamera.getView());
	SortParams params{.eye = math::vec4{toWorld(0, 3), toWorld(1, 3), toWorld(2, 3), 1.f},
					  .forward = math::vec4{-toWorld(0, 2), -toWorld(1, 2), -toWorld(2, 2), 0.f},
					  .emitter = columnsOf(ioSystem.getEmitterMatrix()),
					  .config = {state->capacity, iSettings.space == ParticleSpace::Local ? 1u : 0u, 0, 0}};
	state->sortParams->setData(&params, static_cast<uint32_t>(sizeof(SortParams)), 0);
	g_data->sortKeys->bindStorageBuffer(0, state->sortParams);
	g_data->sortKeys->bindStorageBuffer(1, state->particles);
	g_data->sortKeys->bindStorageBuffer(2, state->order);
	g_data->sortKeys->dispatch(g_sortSize / g_groupSize, 1, 1);
	gpu::RenderCommand::storageBufferMemoryBarrier();
	g_data->sort->bindStorageBuffer(0, state->order);
	g_data->sort->dispatch(1, 1, 1);
	gpu::RenderCommand::storageBufferMemoryBarrier();
}

void RendererParticles::beginScene(const Camera& iCamera) {
	if (!g_data)
		return;
	auto& ubo = g_data->ubo;
	const math::mat4 toWorld = inverse(iCamera.getView());
	const bool perspective = std::abs(iCamera.getProjection()(3, 2)) > 1e-6f;
	ubo.viewProjection = iCamera.getViewProjection();
	ubo.right = math::vec4{toWorld(0, 0), toWorld(1, 0), toWorld(2, 0),
						   gpu::Framebuffer::isLinearTargetBound() ? 1.f : 0.f};
	ubo.up = math::vec4{toWorld(0, 1), toWorld(1, 1), toWorld(2, 1), 0.f};
	ubo.eye = math::vec4{toWorld(0, 3), toWorld(1, 3), toWorld(2, 3), perspective ? 1.f : 0.f};
	ubo.forward = math::vec4{-toWorld(0, 2), -toWorld(1, 2), -toWorld(2, 2), 0.f};
	g_data->sceneOpen = true;
}

namespace {

void submit(const shared<gpu::DrawData>& iDraw, const uint32_t iIndexCount, const uint32_t iInstances,
			const shared<gpu::StorageBuffer>& iParticles, const shared<gpu::StorageBuffer>& iOrder,
			const shared<gpu::StorageBuffer>& iTrails, const shared<gpu::Texture2D>& iTexture) {
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	g_data->uniforms->setData(&g_data->ubo, static_cast<uint32_t>(sizeof(DrawUbo)), 0);
	g_data->uniforms->bind();
	iParticles->bind(2u);
	iOrder->bind(3u);
	iTrails->bind(4u);
	gpu::RenderCommand::beginBatch();
	const std::array textures{iTexture};
	gpu::RenderCommand::bindTextures(textures);
	gpu::RenderCommand::drawDataInstanced(iDraw, iIndexCount, iInstances);
	gpu::RenderCommand::endBatch();
}

}// namespace

void RendererParticles::drawSystem(const ParticleSystem& iSystem, const ParticleEmitterSettings& iSettings,
								   const int32_t iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !g_data->sceneOpen)
		return;
	const auto& state = iSystem.getGpuState();
	if (!state || state->capacity == 0)
		return;
	auto& ubo = g_data->ubo;
	fillTables(ubo, ParticleTables::bake(iSettings));
	const bool sorted = iSettings.blend == ParticleBlend::Alpha;
	const bool local = iSettings.space == ParticleSpace::Local;
	ubo.up.w() = std::max(iSettings.emissive, 0.f) * Renderer3D::getExposure();
	ubo.emitter = columnsOf(iSystem.getEmitterMatrix());
	ubo.config = {state->capacity, sorted ? 1u : 0u, iSettings.texture ? 1u : 0u, local ? 1u : 0u};
	ubo.trail = {state->trailLength, 0, 0, static_cast<uint32_t>(iEntityId)};
	ubo.trailParams = math::vec4{std::max(iSettings.trailWidth, 0.f), 0.f, 0.f, 0.f};
	const auto& texture = iSettings.texture ? iSettings.texture : g_data->white;
	const auto& order = sorted ? state->order : g_data->dummyOrder;
	const size_t blend = drawIndexOf(iSettings.blend);
	if (state->trailLength > 0)
		submit(g_data->trailDraws[blend], state->trailLength * 6, state->capacity, state->particles, order,
			   state->trails, texture);
	submit(g_data->particleDraws[blend], 6, state->capacity, state->particles, order, state->trails, texture);
	++g_data->stats.systems;
	g_data->stats.particles += state->capacity;
}

void RendererParticles::drawTrail(const std::span<const math::vec4> iPoints, const TrailDrawSettings& iSettings,
								  const int32_t iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !g_data->sceneOpen || iPoints.size() < 2 || iSettings.tables == nullptr)
		return;
	const auto count = static_cast<uint32_t>(std::min<size_t>(iPoints.size(), g_maxTrailPoints));
	auto& ubo = g_data->ubo;
	fillTables(ubo, *iSettings.tables);
	ubo.up.w() = std::max(iSettings.emissive, 0.f) * Renderer3D::getExposure();
	ubo.config = {0, 0, 0, 0};
	ubo.trail = {0, 1, count, static_cast<uint32_t>(iEntityId)};
	ubo.trailParams = math::vec4{std::max(iSettings.width, 0.f), 0.f, 0.f, 0.f};
	g_data->entityTrail->setData(iPoints.data(), count * static_cast<uint32_t>(sizeof(math::vec4)), 0);
	submit(g_data->trailDraws[drawIndexOf(iSettings.blend)], (count - 1) * 6, 1, g_data->dummyParticles,
		   g_data->dummyOrder, g_data->entityTrail, g_data->white);
	++g_data->stats.trails;
}

void RendererParticles::endScene() {
	if (g_data)
		g_data->sceneOpen = false;
}

auto RendererParticles::getStats() -> Stats { return g_data ? g_data->stats : Stats{}; }

void RendererParticles::resetStats() {
	if (g_data)
		g_data->stats = {};
}

}// namespace owl::renderer
