/**
 * @file RenderAPI.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "RenderAPI.h"

#include "StorageBuffer.h"
#include "app/Application.h"
#include "core/external/glfw3.h"
#include "internal/Descriptors.h"
#include "internal/RendererDescriptors.h"
#include "internal/VulkanHandler.h"

namespace owl::renderer::gpu::vulkan {

RenderAPI::~RenderAPI() {
	auto& vkh = internal::VulkanHandler::get();
	vkh.release();
}

void RenderAPI::init() {
	OWL_PROFILE_FUNCTION()

	const auto& app = app::Application::get();
	const bool extraDebugging = app.getInitParams().useDebugging;

	if (getState() != State::Created)
		return;

	auto& vkh = internal::VulkanHandler::get();
	if (extraDebugging) {
		vkh.activateValidation();
		vkh.activateDebugMessage();
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

void RenderAPI::drawDataInstanced(const shared<DrawData>& iData, const uint32_t iIndexCount,
								  const uint32_t iInstanceCount) {
	if (iInstanceCount == 0)
		return;
	auto& vkh = internal::VulkanHandler::get();
	iData->bind();
	const bool isIndexed = iData->getIndexCount() > 0;
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	vkh.drawData(count, isIndexed, iInstanceCount);
}

void RenderAPI::drawLine(const shared<DrawData>& iData, const uint32_t iIndexCount) {
	auto& vkh = internal::VulkanHandler::get();
	iData->bind();
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	vkh.drawData(count, false);
}

void RenderAPI::drawLineInstanced(const shared<DrawData>& iData, const uint32_t iIndexCount,
								  const uint32_t iInstanceCount) {
	if (iInstanceCount == 0)
		return;
	auto& vkh = internal::VulkanHandler::get();
	iData->bind();
	const bool isIndexed = iData->getIndexCount() > 0;
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	vkh.drawData(count, isIndexed, iInstanceCount);
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

void RenderAPI::nextSubpass() {
	auto& vkh = internal::VulkanHandler::get();
	vkh.nextSubpass();
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
	if (auto* const rd = internal::RendererDescriptors::getActive(); rd != nullptr) {
		rd->resetTextureBind();
		return;
	}
	auto& vkd = internal::Descriptors::get();
	vkd.resetTextureBind();
}

void RenderAPI::endTextureLoad() {
	const auto frame = internal::VulkanHandler::get().getCurrentFrameIndex();
	if (auto* const rd = internal::RendererDescriptors::getActive(); rd != nullptr) {
		rd->commitTextureBind(frame);
		return;
	}
	auto& vkd = internal::Descriptors::get();
	vkd.commitTextureBind(frame);
}

void RenderAPI::setDepthMask([[maybe_unused]] const bool iEnabled) {
	// Vulkan depth write is managed through pipeline state; no-op for now.
}

void RenderAPI::setDepthTest(const bool iEnabled) {
	auto& vkh = internal::VulkanHandler::get();
	vkh.depthTestEnabled = iEnabled;
	// Also apply immediately if a command buffer is active.
	if (const auto& cmd = vkh.getCurrentCommandBuffer(); cmd != nullptr)
		vkCmdSetDepthTestEnable(cmd, iEnabled ? VK_TRUE : VK_FALSE);
}

void RenderAPI::drawIndexedIndirect(const shared<DrawData>& iData, const shared<gpu::StorageBuffer>& iCommandBuffer,
									const shared<gpu::StorageBuffer>& iCountBuffer, const uint32_t iMaxDrawCount) {
	if (!iData || !iCommandBuffer || !iCountBuffer || iMaxDrawCount == 0)
		return;
	auto* const cmd = internal::VulkanHandler::get().getCurrentCommandBuffer();
	if (cmd == nullptr)
		return;
	iData->bind();
	const auto* cmdSsbo = dynamic_cast<const StorageBuffer*>(iCommandBuffer.get());
	const auto* countSsbo = dynamic_cast<const StorageBuffer*>(iCountBuffer.get());
	if (cmdSsbo == nullptr || countSsbo == nullptr || cmdSsbo->getHandle() == nullptr ||
		countSsbo->getHandle() == nullptr) {
		OWL_CORE_WARN("Vulkan: drawIndexedIndirect with non-Vulkan SSBOs.")
		return;
	}
	vkCmdDrawIndexedIndirectCount(cmd, cmdSsbo->getHandle(), 0, countSsbo->getHandle(), 0, iMaxDrawCount,
								  static_cast<uint32_t>(sizeof(uint32_t) * 5));
}

void RenderAPI::storageBufferMemoryBarrier() {
	auto& handler = internal::VulkanHandler::get();
	// No-op outside an active batch — compute dispatches there run on a one-shot CB whose queue wait is the barrier.
	if (!handler.inBatch)
		return;
	auto* const cmd = handler.getCurrentCommandBuffer();
	if (cmd == nullptr)
		return;
	VkMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
							VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
								 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
						 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

}// namespace owl::renderer::gpu::vulkan
