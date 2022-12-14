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
 * @brief Class Framebuffer
 */
class Framebuffer : public ::owl::renderer::Framebuffer {
public:
	/**
	 * @brief Default copy constructor
	 */
	Framebuffer(const Framebuffer &) = default;
	/**
	 * @brief Default move constructor
	 */
	Framebuffer(Framebuffer &&) = default;
	/**
	 * @brief Default copy assignation
	 * @return this
	 */
	Framebuffer &operator=(const Framebuffer &) = default;
	/**
	 * @brief Default move assignation
	 * @return this
	 */
	Framebuffer &operator=(Framebuffer &&) = default;
	/**
	 * @brief Default constructor.
	 */
	explicit Framebuffer(FramebufferSpecification spec);
	/**
	 * @brief Destructor.
	 */
	~Framebuffer() override;
	/**
	 * @brief Invalidate this framebuffer
	 */
	void invalidate();
	/**
	 * @brief Activate the shader on the GPU
	 */
	void bind() override;
	/**
	 * @brief Deactivate the shader on the GPU
	 */
	void unbind() override;
	/**
	 * @brief Change the size of the frame buffer
	 * @param width New width
	 * @param height New height
	 */
	void resize(uint32_t width, uint32_t height) override;
	int readPixel(uint32_t attachmentIndex, int x, int y) override;
	void clearAttachment(uint32_t attachmentIndex, int value) override;
	/**
	 * @brief Get renderer id
	 * @param index The color index
	 * @return The renderer ID
	 */
	[[nodiscard]] uint32_t getColorAttachmentRendererID(uint32_t index = 0) const override {
		OWL_CORE_ASSERT(index < colorAttachments.size(), "ColorAttachment out of bounds")
		return colorAttachments[index]; }
	/**
	 * @brief Get the specs
	 * @return The specs
	 */
	[[nodiscard]] const FramebufferSpecification &getSpecification() const override { return specs; }

private:
	/// The renderer ID
	uint32_t rendererID = 0;
	/// The color attachment
	std::vector<uint32_t> colorAttachments;
	/// The depth attachment
	uint32_t depthAttachment = 0;
	/// The specs
	FramebufferSpecification specs;
	std::vector<FramebufferTextureSpecification> colorAttachmentSpecifications;
	FramebufferTextureSpecification depthAttachmentSpecification = FramebufferTextureFormat::None;
};

}// namespace owl::renderer::opengl
