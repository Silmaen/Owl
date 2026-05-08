/**
 * @file BackgroundRenderer.cpp
 * @author Silmaen
 * @date 02/15/26
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/BackgroundRenderer.h"

#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/RenderCommand.h"

namespace owl::renderer {

namespace {
/**
 * @brief
 *  Vertex layout for the fullscreen quad.
 * All background data is passed through vertex attributes for Vulkan compatibility.
 */
struct BackgroundVertex {
	math::vec3 position;
	math::vec2 texCoord;
	float texIndex;
	math::vec4 color;
	math::vec4 topColor;
	int mode;
	int entityId;
	math::vec4 invVR0;// column 0 of inverseViewRotation
	math::vec4 invVR1;// column 1
	math::vec4 invVR2;// column 2
	math::vec4 invVR3;// column 3
};

/**
 * @brief
 *  Internal data for the background renderer.
 */
struct InternalData {
	shared<gpu::DrawData> drawData;
	bool pending = false;
	BackgroundRenderer::BackgroundData pendingData;
};

shared<InternalData> g_data;

}// namespace

void BackgroundRenderer::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data) {
		OWL_CORE_WARN("BackgroundRenderer already initiated.")
		g_data.reset();
	}
	g_data = mkShared<InternalData>();

	std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

	g_data->drawData = gpu::DrawData::create();
	g_data->drawData->init(
			{
					{"i_Position", gpu::ShaderDataType::Float3},
					{"i_TexCoord", gpu::ShaderDataType::Float2},
					{"i_TexIndex", gpu::ShaderDataType::Float},
					{"i_Color", gpu::ShaderDataType::Float4},
					{"i_TopColor", gpu::ShaderDataType::Float4},
					{"i_Mode", gpu::ShaderDataType::Int},
					{"i_EntityID", gpu::ShaderDataType::Int},
					{"i_InvVR0", gpu::ShaderDataType::Float4},
					{"i_InvVR1", gpu::ShaderDataType::Float4},
					{"i_InvVR2", gpu::ShaderDataType::Float4},
					{"i_InvVR3", gpu::ShaderDataType::Float4},
			},
			"background", indices, "background");
}

void BackgroundRenderer::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_data) {
		OWL_CORE_WARN("BackgroundRenderer already shutdown.")
		return;
	}
	g_data->drawData.reset();
	g_data.reset();
}

void BackgroundRenderer::drawBackground(const BackgroundData& iData) {
	OWL_PROFILE_FUNCTION()

	if (!g_data)
		return;

	g_data->pending = true;
	g_data->pendingData = iData;
}

auto BackgroundRenderer::hasPending() -> bool { return g_data && g_data->pending; }

auto BackgroundRenderer::getPendingTexture() -> shared<gpu::Texture2D> {
	if (!g_data || !g_data->pending)
		return nullptr;
	const auto& data = g_data->pendingData;
	if (data.texture && (data.mode == 2 || data.mode == 3))
		return data.texture;
	return nullptr;
}

void BackgroundRenderer::flushPending(const float iTexIndex) {
	OWL_PROFILE_FUNCTION()

	if (!g_data || !g_data->pending)
		return;

	const auto& data = g_data->pendingData;

	gpu::RenderCommand::setDepthMask(false);

	// Extract matrix columns
	const auto& m = data.inverseViewRotation;
	const math::vec4 col0 = m.column(0);
	const math::vec4 col1 = m.column(1);
	const math::vec4 col2 = m.column(2);
	const math::vec4 col3 = m.column(3);

	// Upload fullscreen quad vertices: NDC (-1,-1) to (1,1), UVs (0,0) to (1,1)
	const std::array<BackgroundVertex, 4> vertices = {
			BackgroundVertex{{-1.0f, -1.0f, 0.0f},
							 {0.0f, 0.0f},
							 iTexIndex,
							 data.color,
							 data.topColor,
							 data.mode,
							 -1,
							 col0,
							 col1,
							 col2,
							 col3},
			BackgroundVertex{{1.0f, -1.0f, 0.0f},
							 {1.0f, 0.0f},
							 iTexIndex,
							 data.color,
							 data.topColor,
							 data.mode,
							 -1,
							 col0,
							 col1,
							 col2,
							 col3},
			BackgroundVertex{{1.0f, 1.0f, 0.0f},
							 {1.0f, 1.0f},
							 iTexIndex,
							 data.color,
							 data.topColor,
							 data.mode,
							 -1,
							 col0,
							 col1,
							 col2,
							 col3},
			BackgroundVertex{{-1.0f, 1.0f, 0.0f},
							 {0.0f, 1.0f},
							 iTexIndex,
							 data.color,
							 data.topColor,
							 data.mode,
							 -1,
							 col0,
							 col1,
							 col2,
							 col3},
	};
	g_data->drawData->setVertexData(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(BackgroundVertex)));

	gpu::RenderCommand::drawData(g_data->drawData, 6);

	gpu::RenderCommand::setDepthMask(true);

	g_data->pending = false;
}

}// namespace owl::renderer
