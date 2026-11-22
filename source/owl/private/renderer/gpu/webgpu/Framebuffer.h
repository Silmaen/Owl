/**
 * @file Framebuffer.h
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Context.h"
#include "core/external/webgpu.h"
#include "renderer/gpu/Framebuffer.h"

#include <cstdint>
#include <vector>

namespace owl::renderer::gpu::webgpu {

class Texture2D;

/**
 * @brief
 *  WebGPU framebuffer: textures the draws go to while it is bound, sampled afterwards. The swapchain one
 *  (`swapChainTarget`) is the canvas.
 */
class OWL_API Framebuffer final : public renderer::gpu::Framebuffer {
public:
	Framebuffer(const Framebuffer&) = delete;
	Framebuffer(Framebuffer&&) = delete;
	auto operator=(const Framebuffer&) -> Framebuffer& = delete;
	auto operator=(Framebuffer&&) -> Framebuffer& = delete;

	/**
	 * @brief
	 *  Makes the attachments.
	 * @param[in] iSpec The specification.
	 */
	explicit Framebuffer(FramebufferSpecification iSpec);

	/**
	 * @brief
	 *  Destructor.
	 */
	~Framebuffer() override;

	/**
	 * @brief
	 *  The next draws go to the attachments.
	 */
	void bind() override;

	/**
	 * @brief
	 *  The next draws go to the canvas.
	 */
	void unbind() override;

	/**
	 * @brief
	 *  Makes the attachments again for a size.
	 * @param[in] iSize The size.
	 */
	void resize(math::vec2ui iSize) override;

	/**
	 * @brief
	 *  A page reads a texture back asynchronously only: no pixel.
	 * @param[in] iAttachmentIndex Unused.
	 * @param[in] iX Unused.
	 * @param[in] iY Unused.
	 * @return -1.
	 */
	auto readPixel(uint32_t iAttachmentIndex, int iX, int iY) -> int override;

	/**
	 * @brief
	 *  Clears an integer attachment.
	 * @param[in] iAttachmentIndex The attachment.
	 * @param[in] iValue The value.
	 */
	void clearAttachment(uint32_t iAttachmentIndex, int iValue) override;

	/**
	 * @brief
	 *  Clears a colour attachment.
	 * @param[in] iAttachmentIndex The attachment.
	 * @param[in] iColorValue The colour.
	 */
	void clearAttachment(uint32_t iAttachmentIndex, math::vec4 iColorValue) override;

	/**
	 * @brief
	 *  Identifier of a colour attachment.
	 * @param[in] iIndex The attachment.
	 * @return Its texture identifier, 0 for none.
	 */
	[[nodiscard]] auto getColorAttachmentRendererId(uint32_t iIndex) const -> uint64_t override;

	/**
	 * @brief
	 *  The specification.
	 * @return The specification.
	 */
	[[nodiscard]] auto getSpecification() const -> const FramebufferSpecification& override { return m_spec; }

	/**
	 * @brief
	 *  An attachment as a texture.
	 * @param[in] iIndex The attachment.
	 * @return The texture, or nullptr.
	 */
	[[nodiscard]] auto getAttachmentTexture(uint32_t iIndex) -> shared<renderer::gpu::Texture2D> override;

	/**
	 * @brief
	 *  Copies an attachment into the same attachment of another framebuffer of the same size.
	 * @param[in] iIndex The attachment.
	 * @param[in,out] ioTarget The other framebuffer.
	 */
	void copyAttachment(uint32_t iIndex, renderer::gpu::Framebuffer& ioTarget) override;

	/**
	 * @brief
	 *  Copy a colour attachment into one face of one level of a cube.
	 * @param[in] iIndex Index of the colour attachment.
	 * @param[in,out] ioTarget The cube receiving the face.
	 * @param[in] iFace The face, 0 to 5.
	 * @param[in] iLevel The mipmap level of the cube.
	 * @return True when the copy is submitted.
	 */
	auto copyAttachmentToCube(uint32_t iIndex, renderer::gpu::TextureCube& ioTarget, uint32_t iFace, uint32_t iLevel)
			-> bool override;

	/**
	 * @brief
	 *  The attachments the draws go to.
	 * @return The target.
	 */
	[[nodiscard]] auto getTarget() const -> Target;

private:
	/**
	 * @brief
	 *  Makes the attachments from the specification.
	 */
	void create();

	/// The specification.
	FramebufferSpecification m_spec;
	/// One texture per attachment (null for the surface).
	std::vector<wgpu::Texture> m_textures;
	/// Their views.
	std::vector<wgpu::TextureView> m_views;
	/// Their formats.
	std::vector<wgpu::TextureFormat> m_formats;
	/// Their wrappers for sampling, made on demand.
	std::vector<shared<Texture2D>> m_wrappers;
	/// True for the swapchain framebuffer, whose `Surface` attachment is the canvas.
	bool m_surface = false;
};

}// namespace owl::renderer::gpu::webgpu
