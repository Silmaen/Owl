/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>
#include <string>
#include <vector>

namespace owl::renderer::gpu {

class Texture2D;
/**
 * @brief
 *  Specification for the attachment Framebuffer.
 */
struct AttachmentSpecification {
	/// Format for the texture frame buffer.
	enum struct Format : uint8_t {
		None = 0,///< No format.
		/// Colour.
		Rgba8,
		/// Single integer.
		RedInteger,
		/// Depth/stencil.
		Depth32F,
		/// Surface for swap chain.
		Surface,
		/// Half-float colour, the linear HDR scene colour.
		Rgba16F,
		/// Single 32-bit float (shadow-map depth).
		RedFloat,
	};

	/// The texture format for the attachment.
	Format format = Format::None;

	/// The tiling attributes.
	enum struct Tiling : uint8_t {
		/// Linear tiling.
		Linear,
		/// Optimal tiling.
		Optimal
	};

	/// The texture's tiling.
	Tiling tiling = Tiling::Optimal;
};

/**
 * @brief
 *  Framebuffer specification.
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
 * @brief
 *  General class for manipulating frame buffer.
 */
class OWL_API Framebuffer {
public:
	Framebuffer() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	Framebuffer(const Framebuffer&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	Framebuffer(Framebuffer&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const Framebuffer&) -> Framebuffer& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(Framebuffer&&) -> Framebuffer& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~Framebuffer();

	/**
	 * @brief
	 *  Activate the shader on the GPU.
	 */
	virtual void bind() = 0;

	/**
	 * @brief
	 *  Deactivate the shader on the GPU.
	 */
	virtual void unbind() = 0;

	/**
	 * @brief
	 *  Change the size of the frame buffer.
	 * @param[in] iWidth New width.
	 * @param[in] iHeight New height.
	 */
	virtual void resize(uint32_t iWidth, uint32_t iHeight) { resize({iWidth, iHeight}); }

	/**
	 * @brief
	 *  Change the size of the frame buffer.
	 * @param[in] iSize New size.
	 */
	virtual void resize(math::vec2ui iSize) = 0;

	/**
	 * @brief
	 *  Get the Pixel information (picking).
	 * A backend may answer from an asynchronous read (OpenGL, Vulkan): the value can lag the request by a frame or two, so
	 * the caller checks that what it designates still exists.
	 * @param[in] iAttachmentIndex Index in the attachment.
	 * @param[in] iX Horizontal coordinate.
	 * @param[in] iY Vertical coordinate.
	 * @return Pixel index.
	 */
	virtual auto readPixel(uint32_t iAttachmentIndex, int iX, int iY) -> int = 0;

	/**
	 * @brief
	 *  Read back a whole 8-bit colour attachment (`Surface` or `Rgba8`) of an offscreen framebuffer, after its
	 *  `unbind()`. Waits for the GPU: meant for captures and image tests, never for a frame loop.
	 * @param[in] iAttachmentIndex Index of the attachment.
	 * @return `width * height * 4` bytes, RGBA, top row first; empty when the attachment cannot be read back.
	 */
	[[nodiscard]] virtual auto readColorAttachment([[maybe_unused]] uint32_t iAttachmentIndex) -> std::vector<uint8_t> {
		return {};
	}

	/**
	 * @brief
	 *  Reset an attachment with the given value.
	 * @param[in] iAttachmentIndex Index of the attachment.
	 * @param[in] iValue The new value to affect.
	 */
	virtual void clearAttachment(uint32_t iAttachmentIndex, int iValue) = 0;

	/**
	 * @brief
	 *  Clear Attachment.
	 * @param[in] iAttachmentIndex Attachment's index.
	 * @param[in] iColorValue Clearing colour value.
	 */
	virtual void clearAttachment([[maybe_unused]] uint32_t iAttachmentIndex, [[maybe_unused]] math::vec4 iColorValue) {}

	/**
	 * @brief
	 *  Get renderer id.
	 * @param[in] iIndex The colour Index.
	 * @return The renderer ID.
	 */
	[[nodiscard]] virtual auto getColorAttachmentRendererId(uint32_t iIndex) const -> uint64_t = 0;

	/**
	 * @brief
	 *  Returns true if the first byte is the lowest corner.
	 * @return True if the data starts by the bottom.
	 */
	[[nodiscard]] virtual auto isUpsideDown() const -> bool { return false; }

	/**
	 * @brief
	 *  Get the lower point of the data.
	 * @return The lower point of the data.
	 */
	[[nodiscard]] virtual auto getLowerData() const -> math::vec2 { return {0, 0}; }

	/**
	 * @brief
	 *  Get the upper point of the data.
	 * @return The upper point of the data.
	 */
	[[nodiscard]] virtual auto getUpperData() const -> math::vec2 { return {1, 1}; }

	/**
	 * @brief
	 *  Get the specs.
	 * @return The specs.
	 */
	[[nodiscard]] virtual auto getSpecification() const -> const FramebufferSpecification& = 0;

	/**
	 * @brief
	 *  A colour attachment as a texture that draws can sample, after the `unbind()` of the framebuffer. It follows
	 *  the resizes of the framebuffer; it is valid while the framebuffer lives.
	 * @param[in] iIndex Index of the colour attachment.
	 * @return The texture, null when the backend cannot sample the attachment.
	 */
	[[nodiscard]] virtual auto getAttachmentTexture([[maybe_unused]] uint32_t iIndex) -> shared<Texture2D> {
		return nullptr;
	}

	/**
	 * @brief
	 *  Copy an attachment into the same attachment of another framebuffer of the same size and format. Recorded in
	 *  the frame, outside any pass.
	 * @param[in] iIndex Index of the attachment, colour or depth.
	 * @param[in,out] ioTarget Framebuffer receiving the copy.
	 */
	virtual void copyAttachment([[maybe_unused]] uint32_t iIndex, [[maybe_unused]] Framebuffer& ioTarget) {}

	/**
	 * @brief
	 *  Check whether the first attachment stores linear colour (`Rgba16F`): draws into it decode authored sRGB
	 *  colours and leave the encoding to the end of the frame.
	 * @return True for a linear colour framebuffer.
	 */
	[[nodiscard]] auto isLinearColor() const -> bool;

	/**
	 * @brief
	 *  The off-screen framebuffer bound for drawing.
	 * @return The framebuffer, null when the draws go to the window surface.
	 */
	[[nodiscard]] static auto getBound() -> Framebuffer* { return s_bound; }

	/**
	 * @brief
	 *  Check whether the draws go to a linear colour framebuffer (see `isLinearColor`).
	 * @return True when the bound framebuffer stores linear colour.
	 */
	[[nodiscard]] static auto isLinearTargetBound() -> bool { return s_bound != nullptr && s_bound->isLinearColor(); }

	/**
	 * @brief
	 *  Create the frame buffer.
	 * @param[in] iSpec Specifications.
	 * @return The Frame buffer.
	 */
	static auto create(const FramebufferSpecification& iSpec) -> shared<Framebuffer>;

protected:
	/**
	 * @brief
	 *  Record the framebuffer the draws go to; backends call it from `bind()` and `unbind()`.
	 * @param[in] iFramebuffer The bound off-screen framebuffer, null for the window surface.
	 */
	static void setBound(Framebuffer* iFramebuffer) { s_bound = iFramebuffer; }

private:
	/// Off-screen framebuffer bound for drawing, null for the window surface.
	static Framebuffer* s_bound;
};
}// namespace owl::renderer::gpu
