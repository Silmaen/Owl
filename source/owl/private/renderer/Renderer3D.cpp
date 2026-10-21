/**
 * @file Renderer3D.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer3D.h"

#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace owl::renderer {

namespace {
constexpr const char* k_RendererKey = "Renderer3D";
constexpr const char* k_ShaderFolder = "renderer3D";
constexpr uint32_t k_MaxTextureSlots = 32;

// Identifier of this renderer's descriptor block, resolved once.
auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(k_RendererKey);
	return sId;
}

struct SceneUbo {
	math::mat4 viewProjection = math::identity<float, 4>();
	math::mat4 model = math::identity<float, 4>();
	math::vec4 sunDirection{-0.4f, -1.0f, -0.6f, 0.f};
	math::vec4 ambient{0.35f, 0.35f, 0.4f, 1.f};
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	// x: entity id written to the picking attachment; y: 1 when the bound framebuffer stores linear colour; zw: padding.
	std::array<int32_t, 4> ids{-1, 0, 0, 0};
};

static_assert(sizeof(SceneUbo) == 192, "SceneUbo must match the std140 layout of Scene3D in the renderer3D shaders.");

struct LightingUbo {
	// x: light count (0 lights with the sun of the layer), y: cascade count, z: index of the shadowed light.
	std::array<int32_t, 4> counts{0, 0, -1, 0};
	math::vec4 splitDepths{0.f, 0.f, 0.f, 0.f};
	math::vec4 texelWorldSizes{0.f, 0.f, 0.f, 0.f};
	math::vec4 viewDepthRow{0.f, 0.f, 0.f, 0.f};
	// x: normal-offset bias in texels, y: shadow-map size in texels.
	math::vec4 shadowParams{1.5f, 1024.f, 0.f, 0.f};
	std::array<math::mat4, 4> cascades{};
};

static_assert(sizeof(LightingUbo) == 336,
			  "LightingUbo must match the std140 layout of Lighting in the renderer3D shaders.");

struct InternalData {
	shared<gpu::UniformBuffer> sceneUniformBuffer;
	shared<gpu::UniformBuffer> lightingUniformBuffer;
	shared<gpu::StorageBuffer> lightBuffer;
	shared<gpu::Texture2D> whiteTexture;
	SceneUbo scene;
	LightingUbo lighting;
	std::array<GpuLight, Renderer3D::g_maxLights> lights{};
	std::array<shared<gpu::Texture2D>, 4> shadowMaps{};
	std::array<shared<gpu::Texture2D>, k_MaxTextureSlots> textureSlots;
	bool shadowPass = false;
};

shared<InternalData> g_Data;

void bindMeshTextures(const std::span<const shared<gpu::Texture2D>> iTextures) {
	auto& slots = g_Data->textureSlots;
	slots[0] = g_Data->whiteTexture;
	size_t count = 1;
	for (const auto& texture: iTextures) {
		if (texture && count < Renderer3D::g_shadowMapSlot)
			slots[count++] = texture;
	}
	if (g_Data->lighting.counts[1] == 0 || g_Data->shadowPass) {
		gpu::RenderCommand::bindTextures(std::span{slots}.first(count));
		return;
	}
	std::fill(slots.begin() + static_cast<std::ptrdiff_t>(count),
			  slots.begin() + static_cast<std::ptrdiff_t>(Renderer3D::g_shadowMapSlot), g_Data->whiteTexture);
	for (size_t index = 0; index < g_Data->shadowMaps.size(); ++index) {
		const auto& map = g_Data->shadowMaps[index];
		slots[Renderer3D::g_shadowMapSlot + index] = map ? map : g_Data->whiteTexture;
	}
	gpu::RenderCommand::bindTextures(slots);
}

void setDrawParams(const math::mat4& iModel, const MeshDrawParams& iParams) {
	g_Data->scene.model = iModel;
	g_Data->scene.tint = iParams.tint;
	g_Data->scene.ids[0] = iParams.entityId;
	g_Data->scene.ids[2] = g_Data->shadowPass ? 1 : 0;
	g_Data->lightBuffer->bind();
	g_Data->scene.ids[1] = gpu::Framebuffer::isLinearTargetBound() ? 1 : 0;
	g_Data->sceneUniformBuffer->setData(&g_Data->scene, sizeof(SceneUbo), 0);
}
}// namespace

static_assert(sizeof(Mesh3DVertex) == 56, "Mesh3DVertex must stay tightly packed for direct VBO upload.");

void Renderer3D::init() {
	OWL_PROFILE_FUNCTION()

	if (g_Data) {
		OWL_CORE_WARN("Renderer3D already initiated.")
		g_Data.reset();
	}
	g_Data = mkShared<InternalData>();

	const std::array<gpu::BindingDecl, 4> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::VertexFragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = k_MaxTextureSlots,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 2,
							 .type = gpu::BindingType::StorageBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 3,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
	};
	gpu::RendererDescriptors::declare(k_RendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	g_Data->whiteTexture =
			gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {1, 1}, .format = gpu::ImageFormat::Rgba8});
	uint32_t whiteData = 0xffffffff;
	g_Data->whiteTexture->setData(&whiteData, sizeof(uint32_t));

	g_Data->sceneUniformBuffer = gpu::UniformBuffer::create(sizeof(SceneUbo), 0, k_RendererKey);
	g_Data->sceneUniformBuffer->bind();
	g_Data->lightingUniformBuffer = gpu::UniformBuffer::create(sizeof(LightingUbo), 3, k_RendererKey);
	g_Data->lightingUniformBuffer->bind();
	g_Data->lightBuffer = gpu::StorageBuffer::create(g_maxLights * sizeof(GpuLight), 2, k_RendererKey);
	g_Data->lightBuffer->setData(g_Data->lights.data(), static_cast<uint32_t>(sizeof(GpuLight)), 0);
}

void Renderer3D::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_Data) {
		OWL_CORE_WARN("Renderer3D already shutdown.")
		return;
	}
	g_Data->sceneUniformBuffer.reset();
	g_Data->lightingUniformBuffer.reset();
	g_Data->lightBuffer.reset();
	g_Data->shadowMaps = {};
	g_Data->textureSlots = {};
	g_Data->whiteTexture.reset();
	g_Data.reset();
	gpu::RendererDescriptors::release(k_RendererKey);
}

void Renderer3D::beginScene(const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	gpu::RenderCommand::beginBatch();
	g_Data->scene.viewProjection = iCamera.getViewProjection();
	if (g_Data->shadowPass)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	g_Data->lightingUniformBuffer->setData(&g_Data->lighting, sizeof(LightingUbo), 0);
	const auto lightCount = static_cast<uint32_t>(std::max(g_Data->lighting.counts[0], 1));
	g_Data->lightBuffer->setData(g_Data->lights.data(), lightCount * static_cast<uint32_t>(sizeof(GpuLight)), 0);
}

void Renderer3D::endScene() {}

void Renderer3D::setLighting(const math::vec3& iSunDirection, const math::vec3& iAmbient) {
	g_Data->scene.sunDirection = math::vec4{iSunDirection.x(), iSunDirection.y(), iSunDirection.z(), 0.f};
	g_Data->scene.ambient = math::vec4{iAmbient.x(), iAmbient.y(), iAmbient.z(), 1.f};
}

void Renderer3D::setSceneLights(const std::span<const GpuLight> iLights) {
	if (!g_Data)
		return;
	const auto count = std::min(iLights.size(), static_cast<size_t>(g_maxLights));
	std::copy_n(iLights.begin(), count, g_Data->lights.begin());
	g_Data->lighting.counts[0] = static_cast<int32_t>(count);
}

void Renderer3D::setShadow(const ShadowSettings& iShadow) {
	if (!g_Data)
		return;
	auto& lighting = g_Data->lighting;
	const uint32_t count = std::min(iShadow.cascadeCount, static_cast<uint32_t>(lighting.cascades.size()));
	lighting.counts[1] = static_cast<int32_t>(count);
	lighting.counts[2] = count > 0 ? iShadow.lightIndex : -1;
	lighting.splitDepths = iShadow.splitDepths;
	lighting.texelWorldSizes = iShadow.texelWorldSizes;
	lighting.viewDepthRow = iShadow.viewDepthRow;
	lighting.shadowParams = {iShadow.bias, static_cast<float>(iShadow.resolution), 0.f, 0.f};
	lighting.cascades = iShadow.viewProjections;
	g_Data->shadowMaps = iShadow.maps;
}

void Renderer3D::setShadowPass(const bool iShadowPass) {
	if (g_Data)
		g_Data->shadowPass = iShadowPass;
}

auto Renderer3D::isShadowPass() -> bool { return g_Data && g_Data->shadowPass; }

auto Renderer3D::getWhiteTexture() -> shared<gpu::Texture2D> { return g_Data ? g_Data->whiteTexture : nullptr; }

auto Renderer3D::createMesh(std::span<const Mesh3DVertex> iVertices, std::span<const uint32_t> iIndices,
							const std::string& iShaderName, const gpu::PipelineState& iState) -> MeshHandle {
	auto draw = gpu::DrawData::create();
	std::vector<uint32_t> indices{iIndices.begin(), iIndices.end()};
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	draw->init({{"i_Position", gpu::ShaderDataType::Float3},
				{"i_Normal", gpu::ShaderDataType::Float3},
				{"i_Uv", gpu::ShaderDataType::Float2},
				{"i_TexIndex", gpu::ShaderDataType::Int},
				{"i_TileRect", gpu::ShaderDataType::Float4},
				{"i_Ao", gpu::ShaderDataType::Float}},
			   k_ShaderFolder, indices, iShaderName, iState);
	draw->setVertexData(iVertices.data(), static_cast<uint32_t>(iVertices.size() * sizeof(Mesh3DVertex)));
	return draw;
}

void Renderer3D::drawMesh(const MeshHandle& iMesh, const math::mat4& iModel,
						  std::span<const shared<gpu::Texture2D>> iTextures, const MeshDrawParams& iParams) {
	OWL_PROFILE_FUNCTION()

	if (!iMesh || iMesh->getIndexCount() == 0)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	setDrawParams(iModel, iParams);
	bindMeshTextures(iTextures);
	gpu::RenderCommand::drawData(iMesh, iMesh->getIndexCount());
}

void Renderer3D::drawMeshes(std::span<const MeshHandle> iMeshes, const math::mat4& iModel,
							std::span<const shared<gpu::Texture2D>> iTextures, const MeshDrawParams& iParams) {
	OWL_PROFILE_FUNCTION()

	if (iMeshes.empty())
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	setDrawParams(iModel, iParams);
	bindMeshTextures(iTextures);
	for (const auto& mesh: iMeshes) {
		if (mesh && mesh->getIndexCount() > 0)
			gpu::RenderCommand::drawData(mesh, mesh->getIndexCount());
	}
}

}// namespace owl::renderer
