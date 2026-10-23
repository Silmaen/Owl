/**
 * @file OutlinePass.cpp
 * @author Silmaen
 * @date 23/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/OutlinePass.h"

#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace owl::renderer {

namespace {
constexpr auto g_rendererKey = "outline";

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

struct OutlineUbo {
	math::vec4 color{1.f, 0.8f, 0.35f, 1.f};
	// xy: frame size in pixels, z: line width in pixels, w: opacity of the hidden edges.
	math::vec4 frame{1.f, 1.f, 2.f, 0.35f};
	// x: 1 when the scene depth is bound.
	std::array<int32_t, 4> flags{0, 0, 0, 0};
};

struct InternalData {
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> uniformBuffer;
	OutlineUbo params;
};

shared<InternalData> g_data;

}// namespace

void OutlinePass::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data) {
		OWL_CORE_WARN("OutlinePass: Already initiated.")
		g_data.reset();
	}
	g_data = mkShared<InternalData>();
	const std::array<gpu::BindingDecl, 2> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = 2,
							 .stages = gpu::ShaderStage::Fragment},
	};
	gpu::RendererDescriptors::declare(g_rendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};
	g_data->drawData = gpu::DrawData::create();
	g_data->drawData->init({{"i_Position", gpu::ShaderDataType::Float2}}, g_rendererKey, indices, g_rendererKey,
						   {.blendMode = gpu::BlendMode::Alpha, .depthTest = false, .depthWrite = false});
	constexpr std::array<float, 8> corners{-1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f};
	g_data->drawData->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(float)));
	g_data->uniformBuffer = gpu::UniformBuffer::create(sizeof(OutlineUbo), 0, g_rendererKey);
	g_data->uniformBuffer->bind();
}

void OutlinePass::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_data) {
		OWL_CORE_WARN("OutlinePass: Already shutdown.")
		return;
	}
	g_data->drawData.reset();
	g_data->uniformBuffer.reset();
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

void OutlinePass::draw(const OutlineInputs& iInputs) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !iInputs.mask)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	auto& params = g_data->params;
	params.color = iInputs.color;
	params.frame = math::vec4{static_cast<float>(std::max(iInputs.frameSize.x(), 1u)),
							  static_cast<float>(std::max(iInputs.frameSize.y(), 1u)),
							  std::clamp(iInputs.width, 1.f, 8.f), std::clamp(iInputs.hiddenOpacity, 0.f, 1.f)};
	params.flags = {iInputs.sceneDepth ? 1 : 0, 0, 0, 0};
	g_data->uniformBuffer->setData(&params, sizeof(OutlineUbo), 0);
	const std::array textures{iInputs.mask, iInputs.sceneDepth ? iInputs.sceneDepth : iInputs.mask};
	gpu::RenderCommand::bindTextures(textures);
	gpu::RenderCommand::drawData(g_data->drawData, 6);
}

}// namespace owl::renderer
