/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include "core/Core.h"

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
		/// Surface for swap chain.
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
	math::vec2ui size = {0, 0};
	/// Attachment Specs.
	std::vector<AttachmentSpecification> attachments;
	/// Amount of sample.
	uint32_t samples = 1;
	/// If chained target must be swapped.
	bool swapChainTarget = false;
	/// Name that can be used to identify the frame buffer in debug.
	std::string debugName = "main";
};

/**
 * @brief General class for manipulating frame buffer.
 */
class OWL_API Framebuffer {
public:
	Framebuffer() = default;
	Framebuffer(const Framebuffer&) = default;
	Framebuffer(Framebuffer&&) = default;
	auto operator=(const Framebuffer&) -> Framebuffer& = default;
	auto operator=(Framebuffer&&) -> Framebuffer& = default;

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
	virtual void resize(math::vec2ui iSize) = 0;

	/**
	 * Get the Pixel information.
	 * @param[in] iAttachmentIndex Index in the attachment.
	 * @param[in] iX Horizontal coordinate.
	 * @param[in] iY Vertical coordinate.
	 * @return Pixel index.
	 */
	virtual auto readPixel(uint32_t iAttachmentIndex, int iX, int iY) -> int = 0;

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
	[[nodiscard]] virtual auto getColorAttachmentRendererId(uint32_t iIndex) const -> uint64_t = 0;

	/**
	 * @brief Returns true if the first byte is the lowest corner.
	 * @return True if the data starts by the bottom.
	 */
	[[nodiscard]] virtual auto isUpsideDown() const -> bool { return false; }

	/**
	 * @brief Get the lower point of the data.
	 * @return The lower point of the data.
	 */
	[[nodiscard]] virtual auto getLowerData() const -> math::vec2 { return {0, 0}; }
	/**
	 * @brief Get the upper point of the data.
	 * @return The upper point of the data.
	 */
	[[nodiscard]] virtual auto getUpperData() const -> math::vec2 { return {1, 1}; }

	/**
	 * @brief Get the specs.
	 * @return The specs.
	 */
	[[nodiscard]] virtual auto getSpecification() const -> const FramebufferSpecification& = 0;

	/**
	 * @brief Create the frame buffer.
	 * @param[in] iSpec Specifications.
	 * @return The Frame buffer.
	 */
	static auto create(const FramebufferSpecification& iSpec) -> shared<Framebuffer>;

};
}// namespace owl::renderer
