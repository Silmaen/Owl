/**
 * @file ReflectionProbes.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/ReflectionProbes.h"

#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace owl::renderer {

namespace {
constexpr auto g_rendererKey = "probe_filter";
constexpr uint32_t g_smallestProbeFace = 8;

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

struct FilterUbo {
	// x: face, y: mode (0 downsample, 1 GGX), z: level read by a downsample.
	std::array<int32_t, 4> face{0, 0, 0, 0};
	// x: GGX alpha of the level, y: face size of level 0 of the capture, z: last level of the capture.
	math::vec4 params{0.f, 0.f, 0.f, 0.f};
};

struct InternalData {
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> uniformBuffer;
};

shared<InternalData> g_data;

}// namespace

auto getProbeLevelCount(const uint32_t iResolution) -> uint32_t {
	const uint32_t faceLevels = iResolution <= g_smallestProbeFace
										? 1u
										: static_cast<uint32_t>(std::bit_width(iResolution / g_smallestProbeFace));
	return std::clamp(faceLevels, 1u, g_maxProbeLevels);
}

auto getProbeFilterAlpha(const uint32_t iLevel, const uint32_t iLevelCount) -> float {
	if (iLevel == 0 || iLevelCount < 2)
		return 0.f;
	const float roughness = static_cast<float>(iLevel) / static_cast<float>(iLevelCount - 1);
	return roughness * roughness;
}

void ReflectionProbes::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data) {
		OWL_CORE_WARN("ReflectionProbes: Already initiated.")
		g_data.reset();
	}
	g_data = mkShared<InternalData>();
	const std::array<gpu::BindingDecl, 2> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSamplerCube,
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
	g_data->uniformBuffer = gpu::UniformBuffer::create(sizeof(FilterUbo), 0, g_rendererKey);
	g_data->uniformBuffer->bind();
}

void ReflectionProbes::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_data)
		return;
	g_data->drawData.reset();
	g_data->uniformBuffer.reset();
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

auto ReflectionProbes::isReady() -> bool { return g_data != nullptr; }

void ReflectionProbes::drawDownsampledFace(const shared<gpu::TextureCube>& iCapture, const uint32_t iFace,
										   const uint32_t iLevel) {
	drawFace(iCapture, iFace, 0, iLevel);
}

void ReflectionProbes::drawFilteredFace(const shared<gpu::TextureCube>& iCapture, const uint32_t iFace,
										const uint32_t iLevel) {
	drawFace(iCapture, iFace, 1, iLevel);
}

void ReflectionProbes::drawFace(const shared<gpu::TextureCube>& iCapture, const uint32_t iFace, const int32_t iMode,
								const uint32_t iLevel) {
	if (!g_data || !iCapture || iLevel == 0 || iLevel >= iCapture->getLevelCount())
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	const uint32_t levels = iCapture->getLevelCount();
	const FilterUbo params{.face = {static_cast<int32_t>(iFace), iMode, static_cast<int32_t>(iLevel - 1), 0},
						   .params = {getProbeFilterAlpha(iLevel, levels), static_cast<float>(iCapture->getFaceSize(0)),
									  static_cast<float>(levels - 1), 0.f}};
	g_data->uniformBuffer->setData(&params, sizeof(FilterUbo), 0);
	const std::array cubes{iCapture};
	gpu::RenderCommand::bindTextures(std::span<const shared<gpu::Texture2D>>{}, cubes);
	gpu::RenderCommand::drawData(g_data->drawData, 6);
}

}// namespace owl::renderer
