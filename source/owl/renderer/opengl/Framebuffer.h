/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../Framebuffer.h"

namespace owl::renderer::opengl {
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
	 * @brief Get renderer id.
	 * @param[in] iIndex The color index.
	 * @return The renderer ID.
	 */
	[[nodiscard]] uint64_t getColorAttachmentRendererId(const uint32_t iIndex) const override {
		OWL_CORE_ASSERT(iIndex < m_colorAttachments.size(), "ColorAttachment out of bounds")
		return m_colorAttachments[iIndex];
	}

	/**
	 * @brief Returns true if the first byte is the lowest corner.
	 * @return True if the data starts by the bottom.
	 */
	[[nodiscard]] bool isUpsideDown() const override { return true; }

	/**
	 * @brief Get the lower point of the data.
	 * @return The lower point of the data.
	 */
	[[nodiscard]] math::vec2 getLowerData() const override { return {0, 1}; }
	/**
	 * @brief Get the upper point of the data.
	 * @return The upper point of the data.
	 */
	[[nodiscard]] math::vec2 getUpperData() const override { return {1, 0}; }

	/**
	 * @brief Get the specs.
	 * @return The specs.
	 */
	[[nodiscard]] const FramebufferSpecification &getSpecification() const override { return m_specs; }

private:
	/// The renderer ID.
	uint32_t m_rendererId = 0;
	/// The color attachment.
	std::vector<uint32_t> m_colorAttachments;
	/// The depth attachment.
	uint32_t m_depthAttachment = 0;
	/// The specs.
	FramebufferSpecification m_specs;
	std::vector<AttachmentSpecification> m_colorAttachmentSpecifications;
	AttachmentSpecification m_depthAttachmentSpecification = {};
};
}// namespace owl::renderer::opengl
