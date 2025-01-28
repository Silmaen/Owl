/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/Framebuffer.h"

namespace owl::renderer::null {
/**
 * @brief Specialized class for manipulating null frame buffer.
 */
class Framebuffer final : public renderer::Framebuffer {
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
	 * @brief Get renderer id.
	 * @param[in] iIndex The color index.
	 * @return The renderer ID.
	 */
	[[nodiscard]] auto getColorAttachmentRendererId([[maybe_unused]] uint32_t iIndex) const -> uint64_t override {
		return 0;
	}

	/**
	 * @brief Get the specs.
	 * @return The specs.
	 */
	[[nodiscard]] auto getSpecification() const -> const FramebufferSpecification& override { return m_specs; }

private:
	/// The specs.
	FramebufferSpecification m_specs;
};
}// namespace owl::renderer::null
