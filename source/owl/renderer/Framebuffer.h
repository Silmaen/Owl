/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/sizingTypes.h"

namespace owl::renderer {
/**
 * @brief Specification for the attachment Framebuffer.
 */
struct AttachmentSpecification {
	/// Format for the texture frame buffer.
	enum class Format : uint8_t {
		None = 0,
		/// Color.
		Rgba8,
		/// Single integer.
		RedInteger,
		/// Depth/stencil.
		Depth24Stencil8,
		/// Surface for swapchain.
		Surface,
	};

	/// The texture format for the attachment.
	Format format = Format::None;

	/// The tiling attributes.
	enum class Tiling : uint8_t {
		/// Linear tiling.
		Linear,
		/// Optimal tiling.
		Optimal
	};

	/// The texture's tiling.
	Tiling tiling = Tiling::Optimal;
};

/**
 * @brief Framebuffer specification.
 */
struct FramebufferSpecification {
	/// width.
	math::FrameSize size = {0, 0};
	/// Attachment Specs.
	std::vector<AttachmentSpecification> attachments;
	/// Amount of sample.
	uint32_t samples = 1;
	/// If chained target must be swap.
	bool swapChainTarget = false;
	/// Name that can be use to identify the frame buffer in debug.
	std::string debugName = "main";
};

/**
 * @brief Class Framebuffer.
 */
class OWL_API Framebuffer {
public:
	Framebuffer() = default;
	Framebuffer(const Framebuffer &) = default;
	Framebuffer(Framebuffer &&) = default;
	Framebuffer &operator=(const Framebuffer &) = default;
	Framebuffer &operator=(Framebuffer &&) = default;

	/**
	 * @brief Destructor.
	 */
	virtual ~Framebuffer();

	/**
	 * @brief Activate the shader on the GPU.
	 */
	virtual void bind() = 0;

	/**
	 * @brief Deactivate the shader on the GPU.
	 */
	virtual void unbind() = 0;

	/**
	 * @brief Change the size of the frame buffer.
	 * @param[in] iWidth New width.
	 * @param[in] iHeight New height.
	 */
	virtual void resize(uint32_t iWidth, uint32_t iHeight) { resize({iWidth, iHeight}); }

	/**
	 * @brief Change the size of the frame buffer.
	 * @param[in] iSize New size.
	 */
	virtual void resize(math::FrameSize iSize) = 0;

	/**
	 * Get the Pixel information.
	 * @param[in] iAttachmentIndex Index in the attachment.
	 * @param[in] iX Horizontal coordinate.
	 * @param[in] iY Vertical coordinate.
	 * @return Pixel index.
	 */
	virtual int readPixel(uint32_t iAttachmentIndex, int iX, int iY) = 0;

	/**
	 * @brief Reset an attachment with the given value.
	 * @param[in] iAttachmentIndex Index of the attachment.
	 * @param[in] iValue The new value to affect.
	 */
	virtual void clearAttachment(uint32_t iAttachmentIndex, int iValue) = 0;

	/**
	 * @brief Clear Attachment.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iColorValue Clearing color value.
	 */
	virtual void clearAttachment([[maybe_unused]] uint32_t iAttachmentIndex, [[maybe_unused]] math::vec4 iColorValue) {}
	/**
	 * @brief Get renderer id.
	 * @param[in] iIndex The color Index.
	 * @return The renderer ID.
	 */
	[[nodiscard]] virtual uint64_t getColorAttachmentRendererId(uint32_t iIndex) const = 0;

	/**
	 * @brief Returns true if the first byte is the lowest corner.
	 * @return True if the data starts by the bottom.
	 */
	[[nodiscard]] virtual bool isUpsideDown() const { return false; }

	/**
	 * @brief Get the lower point of the data.
	 * @return The lower point of the data.
	 */
	[[nodiscard]] virtual math::vec2 getLowerData() const { return {0, 0}; }
	/**
	 * @brief Get the upper point of the data.
	 * @return The upper point of the data.
	 */
	[[nodiscard]] virtual math::vec2 getUpperData() const { return {1, 1}; }

	/**
	 * @brief Get the specs.
	 * @return The specs.
	 */
	[[nodiscard]] virtual const FramebufferSpecification &getSpecification() const = 0;

	/**
	 * @brief Create the frame buffer.
	 * @param[in] iSpec Specifications.
	 * @return The Frame buffer.
	 */
	static shared<Framebuffer> create(const FramebufferSpecification &iSpec);

private:
};
}// namespace owl::renderer
