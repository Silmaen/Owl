/**
 * @file Renderer3D.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer3D.h"

#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <array>

namespace owl::renderer {

namespace {
constexpr const char* k_RendererKey = "Renderer3D";
constexpr const char* k_ShaderFolder = "renderer3D";
constexpr uint32_t k_MaxTextureSlots = 32;

struct SceneUbo {
	math::mat4 viewProjection = math::identity<float, 4>();
	math::mat4 model = math::identity<float, 4>();
	math::vec4 sunDirection{-0.4f, -1.0f, -0.6f, 0.f};
	math::vec4 ambient{0.35f, 0.35f, 0.4f, 1.f};
};

struct InternalData {
	shared<gpu::UniformBuffer> sceneUniformBuffer;
	shared<gpu::Texture2D> whiteTexture;
	SceneUbo scene;
};

shared<InternalData> g_Data;
}// namespace

static_assert(sizeof(Mesh3DVertex) == 52, "Mesh3DVertex must stay tightly packed for direct VBO upload.");

void Renderer3D::init() {
	OWL_PROFILE_FUNCTION()

	if (g_Data) {
		OWL_CORE_WARN("Renderer3D already initiated.")
		g_Data.reset();
	}
	g_Data = mkShared<InternalData>();

	const std::array<gpu::BindingDecl, 2> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::VertexFragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = k_MaxTextureSlots,
							 .stages = gpu::ShaderStage::Fragment},
	};
	gpu::RendererDescriptors::declare(k_RendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{k_RendererKey};

	g_Data->whiteTexture =
			gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {1, 1}, .format = gpu::ImageFormat::Rgba8});
	uint32_t whiteData = 0xffffffff;
	g_Data->whiteTexture->setData(&whiteData, sizeof(uint32_t));

	g_Data->sceneUniformBuffer = gpu::UniformBuffer::create(sizeof(SceneUbo), 0, k_RendererKey);
	g_Data->sceneUniformBuffer->bind();
}

void Renderer3D::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_Data) {
		OWL_CORE_WARN("Renderer3D already shutdown.")
		return;
	}
	g_Data->sceneUniformBuffer.reset();
	g_Data->whiteTexture.reset();
	g_Data.reset();
	gpu::RendererDescriptors::release(k_RendererKey);
}

void Renderer3D::beginScene(const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	// Open the batch up-front (like Renderer2D::flush) so drawMesh's binds aren't wiped by the lazy beginBatch().
	gpu::RenderCommand::beginBatch();
	g_Data->scene.viewProjection = iCamera.getViewProjection();
}

void Renderer3D::endScene() {}

void Renderer3D::setLighting(const math::vec3& iSunDirection, const math::vec3& iAmbient) {
	g_Data->scene.sunDirection = math::vec4{iSunDirection.x(), iSunDirection.y(), iSunDirection.z(), 0.f};
	g_Data->scene.ambient = math::vec4{iAmbient.x(), iAmbient.y(), iAmbient.z(), 1.f};
}

auto Renderer3D::createMesh(std::span<const Mesh3DVertex> iVertices, std::span<const uint32_t> iIndices,
							const std::string& iShaderName) -> MeshHandle {
	auto draw = gpu::DrawData::create();
	std::vector<uint32_t> indices{iIndices.begin(), iIndices.end()};
	const gpu::RendererDescriptors::ScopedActive scoped{k_RendererKey};
	draw->init({{"i_Position", gpu::ShaderDataType::Float3},
				{"i_Normal", gpu::ShaderDataType::Float3},
				{"i_Uv", gpu::ShaderDataType::Float2},
				{"i_TexIndex", gpu::ShaderDataType::Int},
				{"i_TileRect", gpu::ShaderDataType::Float4}},
			   k_ShaderFolder, indices, iShaderName);
	draw->setVertexData(iVertices.data(), static_cast<uint32_t>(iVertices.size() * sizeof(Mesh3DVertex)));
	return draw;
}

void Renderer3D::drawMesh(const MeshHandle& iMesh, const math::mat4& iModel,
						  std::span<const shared<gpu::Texture2D>> iTextures) {
	OWL_PROFILE_FUNCTION()

	if (!iMesh || iMesh->getIndexCount() == 0)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{k_RendererKey};

	g_Data->scene.model = iModel;
	g_Data->sceneUniformBuffer->setData(&g_Data->scene, sizeof(SceneUbo), 0);
	// Re-assert our scene UBO: siblings share OpenGL uniform binding 0, last-bound wins (no-op on Vulkan).
	g_Data->sceneUniformBuffer->bind();

	gpu::RenderCommand::beginTextureLoad();
	g_Data->whiteTexture->bind(0);
	uint32_t slot = 1;
	for (const auto& texture: iTextures) {
		if (texture && slot < k_MaxTextureSlots) {
			texture->bind(slot);
			++slot;
		}
	}
	gpu::RenderCommand::endTextureLoad();

	iMesh->bind();
	gpu::RenderCommand::setDepthTest(true);
	gpu::RenderCommand::drawData(iMesh, iMesh->getIndexCount());
	gpu::RenderCommand::setDepthTest(false);
}

void Renderer3D::drawMeshes(std::span<const MeshHandle> iMeshes, const math::mat4& iModel,
							std::span<const shared<gpu::Texture2D>> iTextures) {
	OWL_PROFILE_FUNCTION()

	if (iMeshes.empty())
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{k_RendererKey};

	g_Data->scene.model = iModel;
	g_Data->sceneUniformBuffer->setData(&g_Data->scene, sizeof(SceneUbo), 0);
	g_Data->sceneUniformBuffer->bind();

	gpu::RenderCommand::beginTextureLoad();
	g_Data->whiteTexture->bind(0);
	uint32_t slot = 1;
	for (const auto& texture: iTextures) {
		if (texture && slot < k_MaxTextureSlots) {
			texture->bind(slot);
			++slot;
		}
	}
	gpu::RenderCommand::endTextureLoad();

	gpu::RenderCommand::setDepthTest(true);
	for (const auto& mesh: iMeshes) {
		if (!mesh || mesh->getIndexCount() == 0)
			continue;
		mesh->bind();
		gpu::RenderCommand::drawData(mesh, mesh->getIndexCount());
	}
	gpu::RenderCommand::setDepthTest(false);
}

}// namespace owl::renderer
