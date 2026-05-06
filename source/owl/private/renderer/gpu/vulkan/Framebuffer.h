/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <vulkan/vulkan_core.h>

#include "renderer/gpu/Framebuffer.h"

namespace owl::renderer::gpu::vulkan {
/**
 * @brief
 *  Specialized class for manipulating vulkan frame buffer.
 */
class OWL_API Framebuffer final : public renderer::gpu::Framebuffer {
public:
	Framebuffer(const Framebuffer&) = default;

	Framebuffer(Framebuffer&&) = default;

	auto operator=(const Framebuffer&) -> Framebuffer& = default;

	auto operator=(Framebuffer&&) -> Framebuffer& = default;

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
	 *  Get the current framebuffer.
	 * @return The current framebuffer.
	 */
	[[nodiscard]] auto getCurrentFramebuffer() const -> VkFramebuffer { return m_framebuffers[m_currentImage]; }

	/**
	 * @brief
	 *  Get the render pass.
	 * @return The render pass.
	 */
	[[nodiscard]] auto getRenderPass() const -> VkRenderPass { return m_renderPass; }

	/**
	 * @brief
	 *  Get the image count.
	 * @return The image count.
	 */
	[[nodiscard]] auto getImageCount() const -> uint32_t { return m_specs.samples; }

	/**
	 * @brief
	 *  Get the image index.
	 * @return The image index.
	 */
	[[nodiscard]] auto getImageIndex() const -> uint32_t { return m_currentFrame; }

	/**
	 * @brief
	 *  Get the swap chain.
	 * @return The swap chain.
	 */
	[[nodiscard]] auto getSwapChain() const -> VkSwapchainKHR { return m_swapChain; }

	/**
	 * @brief
	 *  Next frame.
	 */
	void nextFrame();

	/**
	 * @brief
	 *  Get the current finished semaphore.
	 * @return The current finished semaphore.
	 */
	[[nodiscard]] auto getCurrentFinishedSemaphore() const -> VkSemaphore {
		return m_samples[m_currentFrame].renderFinishedSemaphore;
	}

	/**
	 * @brief
	 *  Get the current image available semaphore.
	 * @return The current image available semaphore.
	 */
	[[nodiscard]] auto getCurrentImageAvailableSemaphore() const -> VkSemaphore {
		if (m_samples[m_currentFrame].imageAvailableSemaphore != nullptr)
			return m_samples[m_currentFrame].imageAvailableSemaphore;
		return m_samples[m_currentFrame].renderFinishedSemaphore;
	}

	/**
	 * @brief
	 *  Get the current commandbuffer.
	 * @return The current commandbuffer.
	 */
	[[nodiscard]] auto getCurrentCommandbuffer() -> VkCommandBuffer* {
		return &m_samples[m_currentFrame].commandBuffer;
	}

	/**
	 * @brief
	 *  Get the current fence.
	 * @return The current fence.
	 */
	[[nodiscard]] auto getCurrentFence() -> VkFence* { return &m_samples[m_currentFrame].inFlightFence; }

	void setCurrentImage(const uint32_t iImage) { m_currentImage = iImage; }

	/**
	 * @brief
	 *  Get the current image.
	 * @return The current image.
	 */
	[[nodiscard]] auto getCurrentImage() -> uint32_t* { return &m_currentImage; }

	/**
	 * @brief
	 *  Get the name.
	 * @return The name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_specs.debugName; }

	/**
	 * @brief
	 *  Get the color attachment formats.
	 * @return The color attachment formats.
	 */
	[[nodiscard]] auto getColorAttachmentFormats() const -> std::vector<VkFormat>;

	/**
	 * @brief
	 *  Check whether first batch.
	 * @return True when first batch.
	 */
	[[nodiscard]] auto isFirstBatch() const -> bool { return m_firstBatch; }

	/**
	 * @brief
	 *  Reset batch.
	 */
	void resetBatch() { m_firstBatch = true; }

	/**
	 * @brief
	 *  Batch touch.
	 */
	void batchTouch() {
		m_firstBatch = false;
		m_called = true;
	}

	/**
	 * @brief
	 *  Reset sub pass.
	 */
	void resetSubPass() { m_currentSubPass = 0; }

	/**
	 * @brief
	 *  Get the current image ptr.
	 * @return The current image ptr.
	 */
	[[nodiscard]] auto getCurrentImagePtr() const -> VkImage { return m_images[m_currentImage].image; }

	/**
	 * @brief
	 *  Check whether main target.
	 * @return True when main target.
	 */
	[[nodiscard]] auto isMainTarget() const -> bool;

	/**
	 * @brief
	 *  Check whether been called is present.
	 * @return True when been called is present.
	 */
	[[nodiscard]] auto hasBeenCalled() const -> bool;

	/**
	 * @brief
	 *  Get the subpass count.
	 * @return The subpass count.
	 */
	[[nodiscard]] auto getSubpassCount() const -> uint32_t;

	/**
	 * @brief
	 *  Get the current subpass.
	 * @return The current subpass.
	 */
	[[nodiscard]] auto getCurrentSubpass() const -> uint32_t { return m_currentSubPass; }

	/**
	 * @brief
	 *  Next subpass.
	 */
	void nextSubpass();

private:
	/// The specs.
	FramebufferSpecification m_specs;
	/// Vulkan render pass owned by this framebuffer.
	VkRenderPass m_renderPass{};
	/// In-flight frame index (0 .. `MAX_FRAMES_IN_FLIGHT - 1`).
	uint32_t m_currentFrame = 0;
	/// Acquired swapchain image index for this frame.
	uint32_t m_currentImage = 0;
	/// Swapchain handle (only set on the on-screen framebuffer).
	VkSwapchainKHR m_swapChain = nullptr;
	/// Number of images in the swapchain.
	uint32_t m_swapChainImageCount = 0;
	/// True until the first sample is queued in the current frame.
	bool m_firstBatch = true;
	/// True once `begin()` has been called this frame.
	bool m_called = false;
	/// Index of the active subpass within `m_renderPass`.
	uint32_t m_currentSubPass = 0;
	/// Total number of subpasses in `m_renderPass`.
	uint32_t m_SubPassCount = 0;
	// one per sample...
	/**
	 * @brief
	 *  Structure for vulkan sample manipulations.
	 */
	struct Sample {
		VkSemaphore imageAvailableSemaphore;
		VkSemaphore renderFinishedSemaphore;
		VkFence inFlightFence;
		VkCommandBuffer commandBuffer;
	};
	/**
	 * @brief
	 *  Structure for vulkan image manipulation.
	 */
	struct Image {
		VkImage image;
		VkDeviceMemory imageMemory;
		VkImageView imageView;
		VkSampler imageSampler;
		VkDescriptorSet descriptorSet;
		VkDescriptorSetLayout descriptorSetLayout;
	};
	std::vector<Sample> m_samples;
	std::vector<Image> m_images;
	std::vector<VkFramebuffer> m_framebuffers;// need renderpass & all images views created

	/**
	 * @brief
	 *  Cleanup.
	 */
	void cleanup();

	/**
	 * @brief
	 *  Deep cleanup.
	 */
	void deepCleanup();

	/**
	 * @brief
	 *  Create command buffers.
	 */
	void createCommandBuffers();

	/**
	 * @brief
	 *  Create sync objects.
	 */
	void createSyncObjects();

	/**
	 * @brief
	 *  Create images.
	 */
	void createImages();

	/**
	 * @brief
	 *  Create image views.
	 */
	void createImageViews();

	/**
	 * @brief
	 *  Create frame buffer.
	 */
	void createFrameBuffer();

	/**
	 * @brief
	 *  Create render pass.
	 */
	void createRenderPass();

	/**
	 * @brief
	 *  Create descriptor sets.
	 */
	void createDescriptorSets();

	/**
	 * @brief
	 *  Translate an engine attachment slot to its underlying Vulkan image-view index.
	 * @param[in] iAttachmentIndex Attachment slot index (0 = first colour).
	 * @return The matching image-view index.
	 */
	[[nodiscard]] auto attToImgIdx(uint32_t iAttachmentIndex) const -> uint32_t;

	/**
	 * @brief
	 *  Translate a Vulkan image-view index back to its engine attachment slot.
	 * @param[in] iImageIndex Image-view index in the framebuffer.
	 * @return The matching attachment slot.
	 */
	[[nodiscard]] auto imgIdxToAtt(uint32_t iImageIndex) const -> uint32_t;
};
}// namespace owl::renderer::gpu::vulkan
