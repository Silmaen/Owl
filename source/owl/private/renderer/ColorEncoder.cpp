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

#include <algorithm>
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
	math::vec4 post{1.f, 0.f, 0.f, 0.4f};
	math::vec4 vignetteColor{0.f, 0.f, 0.f, 1.f};
	math::vec4 frame{1.f, 1.f, 0.f, 0.f};
	// x: chromatic aberration (0 off), y: film grain (0 off), z: grain size in pixels, w: grain seed of the frame.
	math::vec4 effects{0.f, 0.f, 1.f, 0.f};
};

struct InternalData {
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> uniformBuffer;
	EncodeUbo params;
	// Draws so far, so each frame gets a grain of its own.
	uint32_t drawCount = 0;
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
							 .count = 4,
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

void ColorEncoder::draw(const shared<gpu::Texture2D>& iSceneColor, const ToneMapping iToneMapping,
						const EncodePost& iPost) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !iSceneColor)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	static const PostProcessSettings s_none;
	const auto& settings = iPost.settings != nullptr ? *iPost.settings : s_none;
	auto& params = g_data->params;
	const bool bloom = settings.bloom && iPost.bloom != nullptr;
	const bool lut = iPost.lut != nullptr && iPost.lutSize > 1;
	params.mode = {static_cast<int32_t>(iToneMapping), bloom ? 1 : 0, lut ? 1 : 0, static_cast<int32_t>(iPost.lutSize)};
	params.post = math::vec4{std::exp2(settings.exposure), bloom ? std::max(settings.bloomIntensity, 0.f) : 0.f,
							 settings.vignette ? std::clamp(settings.vignetteIntensity, 0.f, 1.f) : 0.f,
							 std::clamp(settings.vignetteSmoothness, 0.01f, 0.75f)};
	params.vignetteColor = math::vec4{settings.vignetteColor.x(), settings.vignetteColor.y(),
									  settings.vignetteColor.z(), std::clamp(settings.lutContribution, 0.f, 1.f)};
	const bool adapted = iPost.exposure != nullptr;
	params.frame = math::vec4{static_cast<float>(iPost.frameSize.x()), static_cast<float>(iPost.frameSize.y()),
							  settings.ev100, adapted ? 1.f : 0.f};
	params.effects =
			math::vec4{settings.chromaticAberration ? std::clamp(settings.chromaticAberrationIntensity, 0.f, 1.f) : 0.f,
					   settings.filmGrain ? std::clamp(settings.filmGrainIntensity, 0.f, 1.f) : 0.f,
					   std::clamp(settings.filmGrainSize, 1.f, 4.f), static_cast<float>(g_data->drawCount++ % 1024u)};
	g_data->uniformBuffer->setData(&params, sizeof(EncodeUbo), 0);
	const std::array textures{iSceneColor, bloom ? iPost.bloom : iSceneColor, lut ? iPost.lut : iSceneColor,
							  adapted ? iPost.exposure : iSceneColor};
	gpu::RenderCommand::bindTextures(textures);
	gpu::RenderCommand::drawData(g_data->drawData, 6);
}

auto ColorEncoder::srgbToLinear(const float iValue) -> float {
	return iValue <= 0.04045f ? iValue / 12.92f : std::pow((iValue + 0.055f) / 1.055f, 2.4f);
}

}// namespace owl::renderer
