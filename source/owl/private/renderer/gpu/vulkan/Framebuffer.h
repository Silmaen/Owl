/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <vulkan/vulkan_core.h>

#include "internal/MemoryAllocator.h"
#include "internal/VulkanCore.h"
#include "renderer/gpu/Framebuffer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace owl::renderer::gpu::vulkan {
/**
 * @brief
 *  Specialized class for manipulating vulkan frame buffer.
 */
class OWL_API Framebuffer final : public renderer::gpu::Framebuffer {
public:
	Framebuffer(const Framebuffer&) = delete;

	Framebuffer(Framebuffer&&) = delete;

	auto operator=(const Framebuffer&) -> Framebuffer& = delete;

	auto operator=(Framebuffer&&) -> Framebuffer& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iSpec The buffer specifications.
	 */
	explicit Framebuffer(FramebufferSpecification iSpec);

	/**
	 * @brief
	 *  Destructor.
	 */
	~Framebuffer() override;

	/**
	 * @brief
	 *  Invalidate this framebuffer.
	 */
	void invalidate();

	/**
	 * @brief
	 *  Activate the shader on the GPU.
	 */
	void bind() override;

	/**
	 * @brief
	 *  Deactivate the shader on the GPU.
	 */
	void unbind() override;

	/**
	 * @brief
	 *  Change the size of the frame buffer.
	 * @param[in] iSize New size.
	 */
	void resize(math::vec2ui iSize) override;

	/**
	 * @brief
	 *  Get the value of given pixel.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iX X coordinate.
	 * @param[in] iY Y coordinate.
	 * @return Pixel value.
	 */
	auto readPixel(uint32_t iAttachmentIndex, int iX, int iY) -> int override;

	/**
	 * @brief
	 *  Read back a whole 8-bit colour attachment (see the base class).
	 * @param[in] iAttachmentIndex Index of the attachment.
	 * @return RGBA bytes, top row first; empty on failure.
	 */
	[[nodiscard]] auto readColorAttachment(uint32_t iAttachmentIndex) -> std::vector<uint8_t> override;

	/**
	 * @brief
	 *  Clear Attachment.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iValue Clearing value.
	 */
	void clearAttachment(uint32_t iAttachmentIndex, int iValue) override;

	/**
	 * @brief
	 *  Clear Attachment.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iColorValue Clearing colour value.
	 */
	void clearAttachment(uint32_t iAttachmentIndex, math::vec4 iColorValue) override;

	/**
	 * @brief
	 *  Get renderer id.
	 * @param[in] iIndex The colour index.
	 * @return The renderer ID.
	 */
	[[nodiscard]] auto getColorAttachmentRendererId([[maybe_unused]] uint32_t iIndex) const -> uint64_t override;

	/**
	 * @brief
	 *  Get the specs.
	 * @return The specs.
	 */
	[[nodiscard]] auto getSpecification() const -> const FramebufferSpecification& override { return m_specs; }

	/**
	 * @brief
	 *  Get the framebuffer of the current swapchain image (or the only one off-screen).
	 * @return The framebuffer handle.
	 */
	[[nodiscard]] auto getCurrentFramebuffer() const -> VkFramebuffer { return m_framebuffers[m_currentImage]; }

	/**
	 * @brief
	 *  Get the render pass that loads every attachment (pipelines are built against it).
	 * @return The render pass.
	 */
	[[nodiscard]] auto getRenderPass() const -> VkRenderPass { return m_renderPass; }

	/**
	 * @brief
	 *  Pick the render pass of the next batch: the first of a frame clears depth (and the swapchain image), the
	 *  others load every attachment. Both are compatible with the same pipelines.
	 * @param[in] iFrameSerial Serial of the frame being recorded.
	 * @return The render pass to begin.
	 */
	[[nodiscard]] auto acquirePassRenderPass(uint64_t iFrameSerial) -> VkRenderPass;

	/**
	 * @brief
	 *  Check whether the next batch is the first of the frame on this framebuffer (its pass clears on load).
	 * @param[in] iFrameSerial Serial of the frame being recorded.
	 * @return True when no pass of this frame began on this framebuffer yet.
	 */
	[[nodiscard]] auto isFirstPassOf(const uint64_t iFrameSerial) const -> bool { return m_passSerial != iFrameSerial; }

	/**
	 * @brief
	 *  Check a render pass of this frame already ran on the framebuffer.
	 * @param[in] iFrameSerial Serial of the frame being recorded.
	 * @return True once a batch of that frame was opened.
	 */
	[[nodiscard]] auto wasRenderedIn(const uint64_t iFrameSerial) const -> bool { return m_passSerial == iFrameSerial; }

	/**
	 * @brief
	 *  Get the render-pass clear values (depth = 1.0, colours zeroed, the first one of the swapchain set by the caller).
	 * @return The clear values, one per attachment.
	 */
	[[nodiscard]] auto getClearValues() -> std::vector<VkClearValue>& { return m_clearValues; }

	/**
	 * @brief
	 *  Get the number of swapchain images (1 off-screen).
	 * @return The image count.
	 */
	[[nodiscard]] auto getImageCount() const -> uint32_t { return std::max(1u, m_swapChainImageCount); }

	/**
	 * @brief
	 *  Get the swapchain.
	 * @return The swapchain handle, null off-screen.
	 */
	[[nodiscard]] auto getSwapChain() const -> VkSwapchainKHR { return m_swapChain; }

	/**
	 * @brief
	 *  Semaphore signalled when the rendering of a swapchain image is done (one per image, waited by the present).
	 * @param[in] iImage Swapchain image index.
	 * @return The semaphore.
	 */
	[[nodiscard]] auto getRenderFinishedSemaphore(const uint32_t iImage) const -> VkSemaphore {
		return m_renderFinished[iImage];
	}

	/**
	 * @brief
	 *  Set the acquired swapchain image the next passes render into.
	 * @param[in] iImage The image index.
	 */
	void setCurrentImage(const uint32_t iImage) { m_currentImage = iImage; }

	/**
	 * @brief
	 *  Get the acquired swapchain image index.
	 * @return The image index.
	 */
	[[nodiscard]] auto getCurrentImage() const -> uint32_t { return m_currentImage; }

	/**
	 * @brief
	 *  Get the debug name.
	 * @return The name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_specs.debugName; }

	/**
	 * @brief
	 *  Get the formats of the colour attachments.
	 * @return The colour formats, in attachment order.
	 */
	[[nodiscard]] auto getColorAttachmentFormats() const -> std::vector<VkFormat>;

	/**
	 * @brief
	 *  Rewind the subpass counter (a render pass begins).
	 */
	void resetSubPass() { m_currentSubPass = 0; }

	/**
	 * @brief
	 *  Check this is the swapchain framebuffer.
	 * @return True for the swapchain.
	 */
	[[nodiscard]] auto isMainTarget() const -> bool;

	/**
	 * @brief
	 *  Check the framebuffer has a depth attachment.
	 * @return True with a depth attachment.
	 */
	[[nodiscard]] auto hasDepth() const -> bool;

	/**
	 * @brief
	 *  Get the subpass count of the render pass.
	 * @return The subpass count.
	 */
	[[nodiscard]] auto getSubpassCount() const -> uint32_t;

	/**
	 * @brief
	 *  Get the active subpass.
	 * @return The subpass index.
	 */
	[[nodiscard]] auto getCurrentSubpass() const -> uint32_t { return m_currentSubPass; }

	/**
	 * @brief
	 *  Move to the next subpass of the open render pass.
	 */
	void nextSubpass();

	/**
	 * @brief
	 *  Record the transitions of the off-screen colour images to the attachment layout (before a render pass).
	 * @param[in] iCmd Command buffer.
	 */
	void prepareForRendering(VkCommandBuffer iCmd);

	/**
	 * @brief
	 *  Record the transitions of the off-screen colour images to the sampled layout (after the last render pass).
	 * @param[in] iCmd Command buffer.
	 */
	void prepareForSampling(VkCommandBuffer iCmd);

	/**
	 * @brief
	 *  Index of an attachment among the colour attachments of the first subpass.
	 * @param[in] iAttachmentIndex Attachment index.
	 * @return The colour attachment index.
	 */
	[[nodiscard]] auto colorAttachmentIndex(uint32_t iAttachmentIndex) const -> uint32_t;

private:
	/// The specs.
	FramebufferSpecification m_specs;
	/// Render pass loading every attachment (pipelines are built against it).
	VkRenderPass m_renderPass{};
	/// Render pass of the first batch of a frame: depth cleared, swapchain image cleared.
	VkRenderPass m_firstRenderPass{};
	/// Per-attachment render-pass clear values (depth = 1.0, color slots zeroed). Sized to the attachment count.
	std::vector<VkClearValue> m_clearValues;
	/// Acquired swapchain image index for this frame.
	uint32_t m_currentImage = 0;
	/// Swapchain handle (only set on the on-screen framebuffer).
	VkSwapchainKHR m_swapChain = nullptr;
	/// Number of images in the swapchain.
	uint32_t m_swapChainImageCount = 0;
	/// Serial of the last frame a batch was opened in.
	uint64_t m_passSerial = 0;
	/// Index of the active subpass within `m_renderPass`.
	uint32_t m_currentSubPass = 0;
	/// One render-finished semaphore per swapchain image (empty off-screen).
	std::vector<VkSemaphore> m_renderFinished;
	/// Read-back slots of the picking reads, one per frame in flight.
	internal::AllocatedBuffer m_pickBuffer;
	/// Frame serial of the pending pick of each slot (0 when none).
	std::array<uint64_t, internal::g_maxFrameInFlight> m_pickSerial{};
	/// Last pick value read back.
	int m_lastPick = -1;

	/**
	 * @brief
	 *  Structure for vulkan image manipulation.
	 */
	struct Image {
		VkImage image;
		VmaAllocation imageMemory;
		VkImageView imageView;
		VkSampler imageSampler;
		VkDescriptorSet descriptorSet;
		VkDescriptorSetLayout descriptorSetLayout;
		VkImageLayout layout;///< Tracked layout (off-screen colour images).
	};
	/// The images.
	std::vector<Image> m_images;
	/// The framebuffers, one per swapchain image (one off-screen).
	std::vector<VkFramebuffer> m_framebuffers;

	/**
	 * @brief
	 *  Destroy everything, render passes and semaphores included.
	 */
	void deepCleanup();

	/**
	 * @brief
	 *  Destroy the size-dependent objects (images, views, framebuffers, swapchain).
	 */
	void cleanup();

	/**
	 * @brief
	 *  Create the per-image render-finished semaphores of the swapchain.
	 */
	void createSyncObjects();

	/**
	 * @brief
	 *  Destroy the per-image render-finished semaphores.
	 */
	void releaseSyncObjects();

	/**
	 * @brief
	 *  Create the images (and the swapchain).
	 */
	void createImages();

	/**
	 * @brief
	 *  Create the image views.
	 */
	void createImageViews();

	/**
	 * @brief
	 *  Create the framebuffers.
	 */
	void createFrameBuffer();

	/**
	 * @brief
	 *  Create both render passes.
	 */
	void createRenderPass();

	/**
	 * @brief
	 *  Create one render pass.
	 * @param[in] iFirst True for the first-of-frame variant.
	 * @return The render pass.
	 */
	[[nodiscard]] auto buildRenderPass(bool iFirst) const -> VkRenderPass;

	/**
	 * @brief
	 *  Create the descriptor sets ImGui samples the colour attachments with.
	 */
	void createDescriptorSets();

	/**
	 * @brief
	 *  Record a clear of one colour attachment, in the open render pass when this framebuffer is drawn to, in a
	 *  transfer otherwise.
	 * @param[in] iAttachmentIndex Attachment index.
	 * @param[in] iValue Clear value.
	 */
	void recordClear(uint32_t iAttachmentIndex, const VkClearColorValue& iValue);

	/**
	 * @brief
	 *  Convert attachment index into image index.
	 * @param[in] iAttachmentIndex Attachment index.
	 * @return Image index.
	 */
	[[nodiscard]] auto attToImgIdx(uint32_t iAttachmentIndex) const -> uint32_t;

	/**
	 * @brief
	 *  Convert image index into attachment index.
	 * @param[in] iImageIndex Image index.
	 * @return Attachment index.
	 */
	[[nodiscard]] auto imgIdxToAtt(uint32_t iImageIndex) const -> uint32_t;
};
}// namespace owl::renderer::gpu::vulkan
