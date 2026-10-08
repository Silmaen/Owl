/**
 * @file RenderAPI.cpp
 * @author Silmaen
 * @date 30/07/2023
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "RenderAPI.h"

namespace owl::renderer::gpu::null {

void RenderAPI::init() {
	OWL_PROFILE_FUNCTION()

	if (getState() != State::Created)
		return;

	// renderer is now ready
	setState(State::Ready);
}

void RenderAPI::setViewport([[maybe_unused]] const uint32_t iX, [[maybe_unused]] const uint32_t iY,
							[[maybe_unused]] const uint32_t iWidth, [[maybe_unused]] const uint32_t iHeight) {}

void RenderAPI::setClearColor([[maybe_unused]] const math::vec4& iColor) {}

void RenderAPI::clear() {}

void RenderAPI::drawData(const shared<renderer::gpu::DrawData>& iData, [[maybe_unused]] uint32_t iIndexCount) {
	recordDraw(iData);
}

void RenderAPI::drawDataInstanced(const shared<renderer::gpu::DrawData>& iData, [[maybe_unused]] uint32_t iIndexCount,
								  [[maybe_unused]] uint32_t iInstanceCount) {
	recordDraw(iData);
}

void RenderAPI::bindTextures(const std::span<const shared<renderer::gpu::Texture2D>> iTextures) {
	m_boundTextureCount = iTextures.size();
}

void RenderAPI::recordDraw(const shared<renderer::gpu::DrawData>& iData) {
	constexpr size_t maxRecordedDraws = 256;
	if (iData && m_drawnStates.size() < maxRecordedDraws)
		m_drawnStates.push_back(iData->getPipelineState());
}

}// namespace owl::renderer::gpu::null
