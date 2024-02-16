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
#include "internal/VulkanHandler.h"

namespace owl::renderer::vulkan {

RenderAPI::~RenderAPI() {
	auto &vkh = internal::VulkanHandler::get();
	vkh.release();
}

void RenderAPI::init() {
	OWL_PROFILE_FUNCTION()
	const auto &app = core::Application::get();
	const bool extraDebugging = app.getInitParams().useDebugging;

	if (getState() != State::Created) return;

	auto &vkh = internal::VulkanHandler::get();
	if (extraDebugging) {
		vkh.activateValidation();
	}
	vkh.initVulkan();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		setState(State::Error);
		return;
	}

	// renderer is now ready
	setState(State::Ready);
}

void RenderAPI::setViewport(uint32_t, uint32_t, uint32_t, uint32_t) {
	auto &vkh = internal::VulkanHandler::get();
	vkh.setResize();
}

void RenderAPI::setClearColor(const glm::vec4 &color) {
	auto &vkh = internal::VulkanHandler::get();
	vkh.setClearColor(color);
}

void RenderAPI::clear() {
	auto &vkh = internal::VulkanHandler::get();
	vkh.clear();
}

void RenderAPI::drawData(const shared<DrawData> &data, uint32_t index) {
	auto &vkh = internal::VulkanHandler::get();
	data->bind();
	const bool isIndexed = data->getIndexCount() > 0;
	const uint32_t count = index ? index : data->getIndexCount();
	vkh.drawData(count, isIndexed);
}

void RenderAPI::setLineWidth(float) {}

void RenderAPI::beginFrame() {
	auto &vkh = internal::VulkanHandler::get();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_ERROR("Vulkan is in error state ({}).", magic_enum::enum_name(vkh.getState()))
		setState(State::Error);
		return;
	}
	vkh.beginFrame();
}

void RenderAPI::beginBatch() {
	auto &vkh = internal::VulkanHandler::get();
	vkh.beginBatch();
}

void RenderAPI::endBatch() {
	auto &vkh = internal::VulkanHandler::get();
	vkh.endBatch();
}

void RenderAPI::endFrame() {
	auto &vkh = internal::VulkanHandler::get();
	if (vkh.getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_ERROR("Vulkan is in error state: ({}).", magic_enum::enum_name(vkh.getState()))
		setState(State::Error);
		return;
	}
	vkh.endFrame();
}

}// namespace owl::renderer::vulkan
