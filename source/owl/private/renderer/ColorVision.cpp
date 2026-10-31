/**
 * @file ColorVision.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/ColorVision.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace owl::renderer {

namespace {

constexpr auto g_rendererKey = "color_vision";
constexpr auto g_shaderFolder = "color_encode";

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

using Rows = std::array<std::array<float, 3>, 3>;

auto fromRows(const Rows& iRows) -> math::mat3 {
	math::mat3 result;
	for (size_t row = 0; row < 3; ++row)
		for (size_t col = 0; col < 3; ++col) result(row, col) = iRows[row][col];
	return result;
}

auto product(const math::mat3& iLeft, const math::mat3& iRight) -> math::mat3 {
	math::mat3 result;
	for (size_t row = 0; row < 3; ++row)
		for (size_t col = 0; col < 3; ++col)
			for (size_t k = 0; k < 3; ++k) result(row, col) += iLeft(row, k) * iRight(k, col);
	return result;
}

auto identity3() -> math::mat3 { return fromRows({{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}}}); }

// Machado, Oliveira and Fernandes 2009, severity 1, on linear RGB.
auto simulation(const ColorDeficiency iDeficiency) -> math::mat3 {
	switch (iDeficiency) {
		case ColorDeficiency::Protanopia:
			return fromRows({{{0.152286f, 1.052583f, -0.204868f},
							  {0.114503f, 0.786281f, 0.099216f},
							  {-0.003882f, -0.048116f, 1.051998f}}});
		case ColorDeficiency::Deuteranopia:
			return fromRows({{{0.367322f, 0.860646f, -0.227968f},
							  {0.280085f, 0.672501f, 0.047413f},
							  {-0.011820f, 0.042940f, 0.968881f}}});
		case ColorDeficiency::Tritanopia:
			return fromRows({{{1.255528f, -0.076749f, -0.178779f},
							  {-0.078411f, 0.930809f, 0.147602f},
							  {0.004733f, 0.691367f, 0.303900f}}});
		case ColorDeficiency::None:
			break;
	}
	return identity3();
}

// The contrast a protan or deutan loses on red goes to green and blue, the one a tritan loses on blue to red and green.
auto redistribution(const ColorDeficiency iDeficiency) -> math::mat3 {
	if (iDeficiency == ColorDeficiency::Tritanopia)
		return fromRows({{{1.f, 0.f, 0.7f}, {0.f, 1.f, 0.7f}, {0.f, 0.f, 0.f}}});
	return fromRows({{{0.f, 0.f, 0.f}, {0.7f, 1.f, 0.f}, {0.7f, 0.f, 1.f}}});
}

struct VisionUbo {
	math::mat4 matrix;
	math::vec4 frame{1.f, 1.f, 0.f, 0.f};
};

struct InternalData {
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> uniformBuffer;
};

shared<InternalData> g_data;

}// namespace

auto ColorVisionFilter::getMatrix() const -> math::mat3 {
	const math::mat3 identity = identity3();
	if (!isActive())
		return identity;
	const math::mat3 simulated = simulation(deficiency);
	math::mat3 full = simulated;
	if (mode == ColorVisionMode::Correct) {
		math::mat3 lost;
		for (size_t row = 0; row < 3; ++row)
			for (size_t col = 0; col < 3; ++col) lost(row, col) = identity(row, col) - simulated(row, col);
		full = product(redistribution(deficiency), lost);
		for (size_t index = 0; index < 3; ++index) full(index, index) += 1.f;
	}
	const float amount = std::clamp(strength, 0.f, 1.f);
	math::mat3 result;
	for (size_t row = 0; row < 3; ++row)
		for (size_t col = 0; col < 3; ++col)
			result(row, col) = identity(row, col) + amount * (full(row, col) - identity(row, col));
	return result;
}

auto toName(const ColorDeficiency iDeficiency) -> std::string_view {
	switch (iDeficiency) {
		case ColorDeficiency::None:
			return "none";
		case ColorDeficiency::Protanopia:
			return "protanopia";
		case ColorDeficiency::Deuteranopia:
			return "deuteranopia";
		case ColorDeficiency::Tritanopia:
			return "tritanopia";
	}
	return "none";
}

auto colorDeficiencyFromName(const std::string_view iName) -> std::optional<ColorDeficiency> {
	for (const auto deficiency: {ColorDeficiency::None, ColorDeficiency::Protanopia, ColorDeficiency::Deuteranopia,
								 ColorDeficiency::Tritanopia})
		if (toName(deficiency) == iName)
			return deficiency;
	return std::nullopt;
}

void ColorVision::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data) {
		OWL_CORE_WARN("ColorVision already initiated.")
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
	g_data->drawData->init({{"i_Position", gpu::ShaderDataType::Float2}}, g_shaderFolder, indices, g_rendererKey,
						   {.blendMode = gpu::BlendMode::Opaque, .depthTest = false, .depthWrite = false});
	constexpr std::array<float, 8> corners{-1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f};
	g_data->drawData->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(float)));
	g_data->uniformBuffer = gpu::UniformBuffer::create(sizeof(VisionUbo), 0, g_rendererKey);
	g_data->uniformBuffer->bind();
}

void ColorVision::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_data) {
		OWL_CORE_WARN("ColorVision already shutdown.")
		return;
	}
	g_data->drawData.reset();
	g_data->uniformBuffer.reset();
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

void ColorVision::draw(const shared<gpu::Texture2D>& iDisplay, const ColorVisionFilter& iFilter,
					   const math::vec2ui& iFrameSize) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !iDisplay)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	const math::mat3 matrix = iFilter.getMatrix();
	VisionUbo params;
	for (size_t row = 0; row < 3; ++row)
		for (size_t col = 0; col < 3; ++col) params.matrix(row, col) = matrix(row, col);
	params.matrix(3, 3) = 1.f;
	params.frame = math::vec4{static_cast<float>(iFrameSize.x()), static_cast<float>(iFrameSize.y()), 0.f, 0.f};
	g_data->uniformBuffer->setData(&params, sizeof(VisionUbo), 0);
	const std::array textures{iDisplay};
	gpu::RenderCommand::bindTextures(textures);
	gpu::RenderCommand::drawData(g_data->drawData, 6);
}

}// namespace owl::renderer
