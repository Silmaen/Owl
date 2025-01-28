/**
 * @file RenderAPI.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "RenderAPI.h"

#include "core/Application.h"
#include "core/external/glfw3.h"
#include "internal/Descriptors.h"
#include "internal/VulkanHandler.h"

namespace owl::renderer::vulkan {

RenderAPI::~RenderAPI() {
	auto& vkh = internal::VulkanHandler::get();
	vkh.release();
}

void RenderAPI::init() {
	OWL_PROFILE_FUNCTION()
	const auto& app = core::Application::get();
	const bool extraDebugging = app.getInitParams().useDebugging;

	if (getState() != State::Created)
		return;

	auto& vkh = internal::VulkanHandler::get();
	if (extraDebugging)
		vkh.activateValidation();
	vkh.initVulkan();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		setState(State::Error);
		return;
	}

	// renderer is now ready
	setState(State::Ready);
}

void RenderAPI::setViewport(uint32_t, uint32_t, uint32_t, uint32_t) {
	auto& vkh = internal::VulkanHandler::get();
	vkh.setResize();
}

void RenderAPI::setClearColor(const math::vec4& iColor) {
	auto& vkh = internal::VulkanHandler::get();
	vkh.setClearColor(iColor);
}

void RenderAPI::clear() {
	const auto& vkh = internal::VulkanHandler::get();
	vkh.clear();
}

void RenderAPI::drawData(const shared<DrawData>& iData, const uint32_t iIndexCount) {
	auto& vkh = internal::VulkanHandler::get();
	iData->bind();
	const bool isIndexed = iData->getIndexCount() > 0;
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	vkh.drawData(count, isIndexed);
}

void RenderAPI::drawLine(const shared<DrawData>& iData, const uint32_t iIndexCount) {
	auto& vkh = internal::VulkanHandler::get();
	iData->bind();
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	vkh.drawData(count, false);
}

void RenderAPI::beginFrame() {
	auto& vkh = internal::VulkanHandler::get();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_ERROR("Vulkan is in error state ({}).", magic_enum::enum_name(vkh.getState()))
		setState(State::Error);
		return;
	}
	vkh.beginFrame();
}

void RenderAPI::beginBatch() {
	auto& vkh = internal::VulkanHandler::get();
	vkh.beginBatch();
}

void RenderAPI::endBatch() {
	auto& vkh = internal::VulkanHandler::get();
	vkh.endBatch();
}

void RenderAPI::endFrame() {
	auto& vkh = internal::VulkanHandler::get();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_ERROR("Vulkan is in error state: ({}).", magic_enum::enum_name(vkh.getState()))
		setState(State::Error);
		return;
	}
	vkh.endFrame();
}

void RenderAPI::beginTextureLoad() {
	auto& vkd = internal::Descriptors::get();
	vkd.resetTextureBind();
}

void RenderAPI::endTextureLoad() {
	auto& vkd = internal::Descriptors::get();
	vkd.commitTextureBind(internal::VulkanHandler::get().getCurrentFrameIndex());
}

}// namespace owl::renderer::vulkan
