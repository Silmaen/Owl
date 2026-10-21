/**
 * @file ColorEncoder.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/ColorEncoder.h"

#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace owl::renderer {

namespace {
constexpr auto g_rendererKey = "color_encode";

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

struct EncodeUbo {
	std::array<int32_t, 4> mode{0, 0, 0, 0};
};

struct InternalData {
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> uniformBuffer;
	EncodeUbo params;
};

shared<InternalData> g_data;

}// namespace

void ColorEncoder::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data) {
		OWL_CORE_WARN("ColorEncoder already initiated.")
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
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
	};
	gpu::RendererDescriptors::declare(g_rendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};
	g_data->drawData = gpu::DrawData::create();
	g_data->drawData->init({{"i_Position", gpu::ShaderDataType::Float2}}, g_rendererKey, indices, g_rendererKey,
						   {.blendMode = gpu::BlendMode::Opaque, .depthTest = false, .depthWrite = false});
	constexpr std::array<float, 8> corners{-1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f};
	g_data->drawData->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(float)));
	g_data->uniformBuffer = gpu::UniformBuffer::create(sizeof(EncodeUbo), 0, g_rendererKey);
	g_data->uniformBuffer->bind();
}

void ColorEncoder::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_data) {
		OWL_CORE_WARN("ColorEncoder already shutdown.")
		return;
	}
	g_data->drawData.reset();
	g_data->uniformBuffer.reset();
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

void ColorEncoder::draw(const shared<gpu::Texture2D>& iSceneColor, const ToneMapping iToneMapping) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !iSceneColor)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	g_data->params.mode[0] = static_cast<int32_t>(iToneMapping);
	g_data->uniformBuffer->setData(&g_data->params, sizeof(EncodeUbo), 0);
	const std::array textures{iSceneColor};
	gpu::RenderCommand::bindTextures(textures);
	gpu::RenderCommand::drawData(g_data->drawData, 6);
}

auto ColorEncoder::srgbToLinear(const float iValue) -> float {
	return iValue <= 0.04045f ? iValue / 12.92f : std::pow((iValue + 0.055f) / 1.055f, 2.4f);
}

}// namespace owl::renderer
