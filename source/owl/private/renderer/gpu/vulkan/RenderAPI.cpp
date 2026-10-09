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
#include "Texture.h"
#include "app/Application.h"
#include "core/external/glfw3.h"
#include "internal/Descriptors.h"
#include "internal/FrameProfiler.h"
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
	auto& vkh = internal::VulkanHandler::get();
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

void RenderAPI::bindTextures(const std::span<const shared<renderer::gpu::Texture2D>> iTextures) {
	thread_local std::vector<uint32_t> slots;
	slots.clear();
	for (const auto& texture: iTextures)
		slots.push_back(texture ? static_cast<const Texture2D*>(texture.get())->getTextureId() : 0u);
	if (auto* const rd = internal::RendererDescriptors::getActive(); rd != nullptr) {
		rd->setTextures(slots);
		return;
	}
	auto& vkd = internal::Descriptors::get();
	vkd.setTextures(slots, internal::VulkanHandler::get().getCurrentFrameIndex());
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
	// The render pass stays open for the next batch on the same framebuffer (see the declaration).
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


void RenderAPI::drawIndexedIndirect(const shared<DrawData>& iData, const shared<gpu::StorageBuffer>& iCommandBuffer,
									const shared<gpu::StorageBuffer>& iCountBuffer, const uint32_t iMaxDrawCount) {
	if (!iData || !iCommandBuffer || !iCountBuffer || iMaxDrawCount == 0)
		return;
	auto& vkh = internal::VulkanHandler::get();
	auto* const cmdSsbo = dynamic_cast<StorageBuffer*>(iCommandBuffer.get());
	auto* const countSsbo = dynamic_cast<StorageBuffer*>(iCountBuffer.get());
	if (cmdSsbo == nullptr || countSsbo == nullptr) {
		OWL_CORE_WARN("Vulkan: drawIndexedIndirect with non-Vulkan SSBOs.")
		return;
	}
	const auto commands = cmdSsbo->resolve();
	const auto count = countSsbo->resolve();
	if (commands.buffer == nullptr || count.buffer == nullptr)
		return;
	iData->bind();
	if (!vkh.inBatch)
		vkh.beginBatch();
	auto* const cmd = vkh.getRenderPassCommandBuffer();
	if (cmd == nullptr)
		return;
	vkCmdDrawIndexedIndirectCount(cmd, commands.buffer, commands.offset, count.buffer, count.offset, iMaxDrawCount,
								  static_cast<uint32_t>(sizeof(uint32_t) * 5));
}

void RenderAPI::storageBufferMemoryBarrier() {
	auto& handler = internal::VulkanHandler::get();
	// The barrier is recorded outside any render pass, between every earlier and every later command.
	if (!handler.isRecording())
		return;
	handler.recordTransfer([](VkCommandBuffer) -> void {});
}

auto RenderAPI::hasGpuTimestamps() const -> bool { return internal::FrameProfiler::get().isSupported(); }

void RenderAPI::setGpuTimestampsEnabled(const bool iEnabled) { internal::FrameProfiler::get().setEnabled(iEnabled); }

auto RenderAPI::getGpuFrameId() const -> uint64_t { return internal::FrameProfiler::get().getFrameId(); }

auto RenderAPI::popGpuFrameTimings() -> std::vector<GpuFrameTiming> {
	return internal::FrameProfiler::get().popTimings();
}

auto RenderAPI::getRenderCounters() const -> RenderCounters { return internal::FrameProfiler::get().getCounters(); }

void RenderAPI::setVSync(const bool iEnabled) { internal::VulkanCore::get().setVSync(iEnabled); }

auto RenderAPI::getPresentMode() const -> std::string {
	const auto& core = internal::VulkanCore::get();
	if (core.getLogicalDevice() == nullptr)
		return "none";
	const VkPresentModeKHR mode = core.getPresentMode();
	if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		return "immediate";
	if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		return "mailbox";
	if (mode == VK_PRESENT_MODE_FIFO_KHR)
		return "fifo";
	return "other";
}

auto RenderAPI::getDeviceName() const -> std::string { return internal::VulkanCore::get().getDeviceName(); }

}// namespace owl::renderer::gpu::vulkan
