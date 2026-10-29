/**
 * @file LensFlare.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/LensFlare.h"

#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace owl::renderer {

namespace {

constexpr auto g_rendererKey = "lens_flare";

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

struct FlareUbo {
	math::mat4 viewProjection = math::identity<float, 4>();
	std::array<math::vec4, LensFlare::g_maxSources> sources{};
	std::array<math::vec4, LensFlare::g_maxSources> colors{};
	math::vec4 params{0.f, 0.f, 0.f, 0.f};
	math::vec4 frame{1.f, 1.f, 1.f, 0.f};
	std::array<uint32_t, 4> counts{0, 0, 0, 0};
};

struct InternalData {
	shared<gpu::DrawData> copyDraw;
	shared<gpu::DrawData> flareDraw;
	shared<gpu::UniformBuffer> uniforms;
	FlareUbo ubo;
	RenderGraph::Resource color;
	RenderGraph::Resource depth;
	uint32_t instances = 0;
};

shared<InternalData> g_data;

}// namespace

void LensFlare::init() {
	OWL_PROFILE_FUNCTION()

	g_data = mkShared<InternalData>();
	const std::array<gpu::BindingDecl, 2> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::VertexFragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = 2,
							 .stages = gpu::ShaderStage::VertexFragment},
	};
	gpu::RendererDescriptors::declare(g_rendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	constexpr std::array<int32_t, 4> corners{0, 1, 2, 3};
	for (const auto blend: {gpu::BlendMode::Opaque, gpu::BlendMode::Additive}) {
		std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};
		auto draw = gpu::DrawData::create();
		draw->init({{"i_CornerIndex", gpu::ShaderDataType::Int}}, g_rendererKey, indices, "lens_flare",
				   {.blendMode = blend, .depthTest = false, .depthWrite = false});
		draw->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(int32_t)));
		(blend == gpu::BlendMode::Opaque ? g_data->copyDraw : g_data->flareDraw) = std::move(draw);
	}
	g_data->uniforms = gpu::UniformBuffer::create(static_cast<uint32_t>(sizeof(FlareUbo)), 0, g_rendererKey);
	g_data->uniforms->bind();
}

void LensFlare::shutdown() {
	if (!g_data)
		return;
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

auto LensFlare::ghostOffset(const uint32_t iGhost, const float iSpacing) -> float {
	return 1.f - iSpacing * static_cast<float>(iGhost + 1);
}

auto LensFlare::brightnessOf(const float iIntensity, const float iDistance, const float iExposure,
							 const float iStrength) -> float {
	const float illuminance = iDistance > 0.f ? iIntensity / std::max(iDistance * iDistance, 1e-2f) : iIntensity;
	return std::clamp(illuminance * iExposure * std::max(iStrength, 0.f), 0.f, g_maxBrightness);
}

auto LensFlare::recordLensFlares(RenderGraph& ioGraph, const RenderGraph::Resource iColor,
								 const RenderGraph::Resource iDepth, const PostProcessSettings& iSettings,
								 const math::mat4& iViewProjection, const std::span<const LensFlareSource> iSources,
								 const math::vec2ui& iFrameSize) -> RenderGraph::Resource {
	using Graph = RenderGraph;
	if (!g_data || !iSettings.lensFlare || !iDepth.isValid() || iSources.empty() || iFrameSize.y() == 0)
		return {};
	auto& ubo = g_data->ubo;
	const auto count = static_cast<uint32_t>(std::min<size_t>(iSources.size(), g_maxSources));
	const float intensity = std::max(iSettings.lensFlareIntensity, 0.f);
	for (uint32_t index = 0; index < count; ++index) {
		const auto& source = iSources[index];
		ubo.sources[index] = math::vec4{source.position.x(), source.position.y(), source.position.z(),
										source.directional ? 0.f : 1.f};
		ubo.colors[index] = math::vec4{source.color.x() * intensity, source.color.y() * intensity,
									   source.color.z() * intensity, 0.f};
	}
	const uint32_t ghosts = std::min(iSettings.lensFlareGhosts, g_maxGhosts);
	ubo.viewProjection = iViewProjection;
	ubo.params = math::vec4{static_cast<float>(ghosts), std::max(iSettings.lensFlareSpacing, 0.f),
							std::max(iSettings.lensFlareHalo, 0.f), std::max(iSettings.lensFlareSize, 0.f)};
	ubo.frame = math::vec4{static_cast<float>(iFrameSize.x()), static_cast<float>(iFrameSize.y()),
						   static_cast<float>(iFrameSize.x()) / static_cast<float>(iFrameSize.y()), 0.f};
	ubo.counts = {count, 0, 0, 0};
	g_data->color = iColor;
	g_data->depth = iDepth;
	g_data->instances = count * (ghosts + 2);
	const auto target = ioGraph.createTarget("lens flares", Graph::TargetFormat::Linear);
	ioGraph.addPass(
			"lens flares",
			[iColor, iDepth, target](Graph::PassBuilder& ioBuilder) -> void {
				ioBuilder.read(iColor);
				ioBuilder.read(iDepth);
				ioBuilder.write(target);
			},
			[](const Graph::PassContext& iContext) -> void {
				if (!g_data)
					return;
				const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
				const std::array textures{iContext.getTexture(g_data->color), iContext.getTexture(g_data->depth)};
				g_data->ubo.frame.w() = 1.f;
				g_data->uniforms->setData(&g_data->ubo, static_cast<uint32_t>(sizeof(FlareUbo)), 0);
				gpu::RenderCommand::bindTextures(textures);
				gpu::RenderCommand::drawData(g_data->copyDraw, 6);
				g_data->ubo.frame.w() = 0.f;
				g_data->uniforms->setData(&g_data->ubo, static_cast<uint32_t>(sizeof(FlareUbo)), 0);
				gpu::RenderCommand::bindTextures(textures);
				gpu::RenderCommand::drawDataInstanced(g_data->flareDraw, 6, g_data->instances);
			});
	return target;
}

}// namespace owl::renderer
