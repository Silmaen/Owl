/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <vulkan/vulkan_core.h>

#include "renderer/Framebuffer.h"

namespace owl::renderer::vulkan {
/**
 * @brief Specialized class for manipulating vulkan frame buffer.
 */
class OWL_API Framebuffer final : public renderer::Framebuffer {
public:
	Framebuffer(const Framebuffer&) = default;
	Framebuffer(Framebuffer&&) = default;
	auto operator=(const Framebuffer&) -> Framebuffer& = default;
	auto operator=(Framebuffer&&) -> Framebuffer& = default;

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
	void resize(math::vec2ui iSize) override;

	/**
	 * @brief Get the value of given pixel.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iX X coordinate.
	 * @param[in] iY Y coordinate.
	 * @return Pixel value.
	 */
	auto readPixel(uint32_t iAttachmentIndex, int iX, int iY) -> int override;

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
	[[nodiscard]] auto getColorAttachmentRendererId([[maybe_unused]] uint32_t iIndex) const -> uint64_t override;

	/**
	 * @brief Get the specs.
	 * @return The specs.
	 */
	[[nodiscard]] auto getSpecification() const -> const FramebufferSpecification& override { return m_specs; }

	[[nodiscard]] auto getCurrentFramebuffer() const -> VkFramebuffer { return m_framebuffers[m_currentImage]; }

	[[nodiscard]] auto getRenderPass() const -> VkRenderPass { return m_renderPass; }

	[[nodiscard]] auto getImageCount() const -> uint32_t { return m_specs.samples; }
	[[nodiscard]] auto getImageIndex() const -> uint32_t { return m_currentFrame; }

	[[nodiscard]] auto getSwapChain() const -> VkSwapchainKHR { return m_swapChain; }

	void nextFrame();

	[[nodiscard]] auto getCurrentFinishedSemaphore() const -> VkSemaphore {
		return m_samples[m_currentFrame].renderFinishedSemaphore;
	}

	[[nodiscard]] auto getCurrentImageAvailableSemaphore() const -> VkSemaphore {
		if (m_samples[m_currentFrame].imageAvailableSemaphore != nullptr)
			return m_samples[m_currentFrame].imageAvailableSemaphore;
		return m_samples[m_currentFrame].renderFinishedSemaphore;
	}

	[[nodiscard]] auto getCurrentCommandbuffer() -> VkCommandBuffer* {
		return &m_samples[m_currentFrame].commandBuffer;
	}
	[[nodiscard]] auto getCurrentFence() -> VkFence* { return &m_samples[m_currentFrame].inFlightFence; }


	void setCurrentImage(const uint32_t iImage) { m_currentImage = iImage; }
	[[nodiscard]] auto getCurrentImage() -> uint32_t* { return &m_currentImage; }

	[[nodiscard]] auto getName() const -> const std::string& { return m_specs.debugName; }

	[[nodiscard]] auto getColorAttachmentFormats() const -> std::vector<VkFormat>;

	[[nodiscard]] auto isFirstBatch() const -> bool { return m_firstBatch; }
	void resetBatch() { m_firstBatch = true; }
	void batchTouch() { m_firstBatch = false; }

	[[nodiscard]] auto getCurrentImagePtr() const -> VkImage { return m_images[m_currentImage].image; }

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
	/**
	 * @brief Structure for vulkan sample manipulations.
	 */
	struct Sample {
		VkSemaphore imageAvailableSemaphore;
		VkSemaphore renderFinishedSemaphore;
		VkFence inFlightFence;
		VkCommandBuffer commandBuffer;
	};

	/**
	 * @brief Structure for vulkan image manipulation.
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

	void cleanup();
	void deepCleanup();
	void createCommandBuffers();
	void createSyncObjects();
	void createImages();
	void createImageViews();
	void createFrameBuffer();
	void createRenderPass();
	void createDescriptorSets();
	[[nodiscard]] auto attToImgIdx(uint32_t iAttachmentIndex) const -> uint32_t;
	[[nodiscard]] auto imgIdxToAtt(uint32_t iImageIndex) const -> uint32_t;
};
}// namespace owl::renderer::vulkan
