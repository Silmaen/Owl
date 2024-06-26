/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <vulkan/vulkan_core.h>

#include "../Framebuffer.h"

namespace owl::renderer::vulkan {
/**
 * @brief Class Framebuffer.
 */
class Framebuffer final : public renderer::Framebuffer {
public:
	Framebuffer(const Framebuffer &) = default;
	Framebuffer(Framebuffer &&) = default;
	Framebuffer &operator=(const Framebuffer &) = default;
	Framebuffer &operator=(Framebuffer &&) = default;

	/**
	 * @brief Default constructor.
	 * @param[in] iSpec The buffer specifications.
	 */
	explicit Framebuffer(FramebufferSpecification iSpec);

	/**
	 * @brief Destructor.
	 */
	~Framebuffer() override;

	/**
	 * @brief Invalidate this framebuffer.
	 */
	void invalidate();

	/**
	 * @brief Activate the shader on the GPU.
	 */
	void bind() override;

	/**
	 * @brief Deactivate the shader on the GPU.
	 */
	void unbind() override;

	/**
	 * @brief Change the size of the frame buffer.
	 * @param[in] iSize New size.
	 */
	void resize(math::FrameSize iSize) override;

	/**
	 * @brief Get the value of given pixel.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iX X coordinate.
	 * @param[in] iY Y coordinate.
	 * @return Pixel value.
	 */
	int readPixel(uint32_t iAttachmentIndex, int iX, int iY) override;

	/**
	 * @brief Clear Attachment.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iValue Clearing value.
	 */
	void clearAttachment(uint32_t iAttachmentIndex, int iValue) override;

	/**
	 * @brief Clear Attachment.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iColorValue Clearing color value.
	 */
	void clearAttachment(uint32_t iAttachmentIndex, math::vec4 iColorValue) override;
	/**
	 * @brief Get renderer id.
	 * @param[in] iIndex The color index.
	 * @return The renderer ID.
	 */
	[[nodiscard]] uint64_t getColorAttachmentRendererId([[maybe_unused]] uint32_t iIndex) const override;

	/**
	 * @brief Get the specs.
	 * @return The specs.
	 */
	[[nodiscard]] const FramebufferSpecification &getSpecification() const override { return m_specs; }

	[[nodiscard]] VkFramebuffer getCurrentFramebuffer() const { return m_framebuffers[m_currentImage]; }

	[[nodiscard]] VkRenderPass getRenderPass() const { return m_renderPass; }

	[[nodiscard]] uint32_t getImageCount() const { return m_specs.samples; }
	[[nodiscard]] uint32_t getImageIndex() const { return m_currentFrame; }

	[[nodiscard]] VkSwapchainKHR getSwapChain() const { return m_swapChain; }

	void nextFrame();

	[[nodiscard]] VkSemaphore getCurrentFinishedSemaphore() const {
		return m_samples[m_currentFrame].renderFinishedSemaphore;
	}

	[[nodiscard]] VkSemaphore getCurrentImageAvailableSemaphore() const {
		if (m_samples[m_currentFrame].imageAvailableSemaphore)
			return m_samples[m_currentFrame].imageAvailableSemaphore;
		return m_samples[m_currentFrame].renderFinishedSemaphore;
	}

	[[nodiscard]] VkCommandBuffer *getCurrentCommandbuffer() { return &m_samples[m_currentFrame].commandBuffer; }
	[[nodiscard]] VkFence *getCurrentFence() { return &m_samples[m_currentFrame].inFlightFence; }


	void setCurrentImage(const uint32_t iImage) { m_currentImage = iImage; }
	[[nodiscard]] uint32_t *getCurrentImage() { return &m_currentImage; }

	[[nodiscard]] const std::string &getName() const { return m_specs.debugName; }

	[[nodiscard]] std::vector<VkFormat> getColorAttachmentformats() const;

	[[nodiscard]] bool isFirstBatch() const { return m_firstBatch; }
	void resetBatch() { m_firstBatch = true; }
	void batchTouch() { m_firstBatch = false; }

	[[nodiscard]] VkImage getCurrentImagePtr() const { return m_images[m_currentImage].image; }

private:
	/// The specs.
	FramebufferSpecification m_specs;

	VkRenderPass m_renderPass{};
	uint32_t m_currentFrame = 0;
	uint32_t m_currentImage = 0;
	VkSwapchainKHR m_swapChain = nullptr;
	uint32_t m_swapChainImageCount = 0;
	bool m_firstBatch = true;

	// one per sample...
	struct Sample {
		VkSemaphore imageAvailableSemaphore;
		VkSemaphore renderFinishedSemaphore;
		VkFence inFlightFence;
		VkCommandBuffer commandBuffer;
	};

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

	void cleanup();
	void deepCleanup();
	void createCommandBuffers();
	void createSyncObjects();
	void createImages();
	void createImageViews();
	void createFrameBuffer();
	void createRenderPass();
	void createDescriptorSets();
	[[nodiscard]] uint32_t attToImgIdx(uint32_t iAttachmentIndex) const;
	[[nodiscard]] uint32_t imgIdxToAtt(uint32_t iImageIndex) const;
};
}// namespace owl::renderer::vulkan
