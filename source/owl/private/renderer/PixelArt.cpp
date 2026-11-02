/**
 * @file PixelArt.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/PixelArt.h"

#include "RenderStackYaml.h"
#include "core/external/yaml.h"
#include "renderer/ColorEncoder.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace owl::renderer {

namespace {

constexpr auto g_rendererKey = "pixel_art";
constexpr uint32_t g_maxHeight = 4096;
constexpr uint32_t g_maxLevels = 256;

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

struct PixelArtUbo {
	// rgb: linear colour of the outline, a: 1 to draw it.
	math::vec4 outline{0.f, 0.f, 0.f, 0.f};
	// xy: frame size in pixels, zw: low-resolution size in texels.
	math::vec4 frame{1.f, 1.f, 1.f, 1.f};
	// x: colour levels per channel.
	math::vec4 params{0.f, 0.f, 0.f, 0.f};
};

struct InternalData {
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> uniformBuffer;
};

shared<InternalData> g_data;

}// namespace

auto PixelArtSettings::getTargetSize(const math::vec2ui& iFrameSize) const -> math::vec2ui {
	if (iFrameSize.x() == 0 || iFrameSize.y() == 0)
		return {1, 1};
	const uint32_t targetHeight = std::clamp(height, 1u, iFrameSize.y());
	const auto width = static_cast<uint32_t>(
			std::lround(static_cast<double>(targetHeight) * iFrameSize.x() / static_cast<double>(iFrameSize.y())));
	return {std::clamp(width, 1u, iFrameSize.x()), targetHeight};
}

auto PixelArtSettings::fromConfig(const std::string& iConfig, const PixelArtSettings& iFallback) -> PixelArtSettings {
	PixelArtSettings settings = iFallback;
	const auto config = parseYamlText(iConfig);
	if (!config || !config.IsMap())
		return settings;
	if (const auto node = config["PixelHeight"]; node)
		settings.height = std::min(node.as<uint32_t>(0), g_maxHeight);
	if (const auto node = config["PixelColorLevels"]; node)
		settings.colorLevels = std::min(node.as<uint32_t>(0), g_maxLevels);
	if (const auto node = config["PixelOutline"]; node && node.IsSequence() && node.size() >= 4)
		settings.outline = {node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>()};
	return settings;
}

void PixelArtPass::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data) {
		OWL_CORE_WARN("PixelArtPass: Already initiated.")
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
						   {.blendMode = gpu::BlendMode::Opaque, .depthTest = true, .depthWrite = true});
	constexpr std::array<float, 8> corners{-1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f};
	g_data->drawData->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(float)));
	g_data->uniformBuffer = gpu::UniformBuffer::create(sizeof(PixelArtUbo), 0, g_rendererKey);
	g_data->uniformBuffer->bind();
}

void PixelArtPass::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_data) {
		OWL_CORE_WARN("PixelArtPass: Already shutdown.")
		return;
	}
	g_data->drawData.reset();
	g_data->uniformBuffer.reset();
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

void PixelArtPass::draw(const Inputs& iInputs) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !iInputs.color || !iInputs.depth)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	const auto& outline = iInputs.settings.outline;
	PixelArtUbo params;
	params.outline = {ColorEncoder::srgbToLinear(outline.r()), ColorEncoder::srgbToLinear(outline.g()),
					  ColorEncoder::srgbToLinear(outline.b()), outline.a() > 0.f ? 1.f : 0.f};
	params.frame = {static_cast<float>(std::max(iInputs.frameSize.x(), 1u)),
					static_cast<float>(std::max(iInputs.frameSize.y(), 1u)),
					static_cast<float>(std::max(iInputs.size.x(), 1u)),
					static_cast<float>(std::max(iInputs.size.y(), 1u))};
	params.params = {static_cast<float>(iInputs.settings.colorLevels), 0.f, 0.f, 0.f};
	g_data->uniformBuffer->setData(&params, sizeof(PixelArtUbo), 0);
	const std::array textures{iInputs.color, iInputs.depth};
	gpu::RenderCommand::bindTextures(textures);
	gpu::RenderCommand::drawData(g_data->drawData, 6);
}

}// namespace owl::renderer
