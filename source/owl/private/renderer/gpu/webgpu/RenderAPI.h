/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/RenderAPI.h"

#include <cstdint>
#include <span>
#include <string>

namespace owl::renderer::gpu::webgpu {

/**
 * @brief
 *  WebGPU backend of the Owl RHI, the one of the Web build.
 */
class OWL_API RenderAPI final : public renderer::gpu::RenderAPI {
public:
	RenderAPI() : renderer::gpu::RenderAPI(Type::WebGpu) {}
	RenderAPI(const RenderAPI&) = delete;
	RenderAPI(RenderAPI&&) = delete;
	auto operator=(const RenderAPI&) -> RenderAPI& = delete;
	auto operator=(RenderAPI&&) -> RenderAPI& = delete;

	/**
	 * @brief
	 *  Destructor: releases the WebGPU objects of the page.
	 */
	~RenderAPI() override;

	/**
	 * @brief
	 *  Ready once the graphics context took the device of the page.
	 */
	void init() override;

	/**
	 * @brief
	 *  Set the view port for the rendering.
	 * @param[in] iX Starting X coordinate.
	 * @param[in] iY Starting Y coordinate.
	 * @param[in] iWidth Width of the view port.
	 * @param[in] iHeight Height of the view port.
	 */
	void setViewport(uint32_t iX, uint32_t iY, uint32_t iWidth, uint32_t iHeight) override;

	/**
	 * @brief
	 *  Define the background color.
	 * @param[in] iColor The new background color.
	 */
	void setClearColor(const math::vec4& iColor) override;

	/**
	 * @brief
	 *  Clear the canvas with the background color.
	 */
	void clear() override;

	/**
	 * @brief
	 *  Draw the data.
	 * @param[in] iData The draw data.
	 * @param[in] iIndexCount Number of indices.
	 */
	void drawData(const shared<renderer::gpu::DrawData>& iData, uint32_t iIndexCount) override;

	/**
	 * @brief
	 *  Draw instances of the data.
	 * @param[in] iData The draw data.
	 * @param[in] iIndexCount Number of indices per instance.
	 * @param[in] iInstanceCount Number of instances.
	 */
	void drawDataInstanced(const shared<renderer::gpu::DrawData>& iData, uint32_t iIndexCount,
						   uint32_t iInstanceCount) override;

	/**
	 * @brief
	 *  Draw one indexed indirect command that a compute pass may have written.
	 * @param[in] iData The draw data.
	 * @param[in] iCommandBuffer Storage buffer of the commands.
	 * @param[in] iCommandIndex Index of the command in the buffer.
	 */
	void drawIndexedIndirectAt(const shared<renderer::gpu::DrawData>& iData,
							   const shared<renderer::gpu::StorageBuffer>& iCommandBuffer,
							   uint32_t iCommandIndex) override;

	/**
	 * @brief
	 *  Texture slots of a draw.
	 * @return The slot count.
	 */
	[[nodiscard]] auto getMaxTextureSlots() const -> uint32_t override;

	/**
	 * @brief
	 *  Tells whether WebGPU samples a format without a feature the page does not ask for.
	 * @param[in] iFormat The format.
	 * @return False for the compressed formats.
	 */
	[[nodiscard]] auto supportsFormat(ImageFormat iFormat) const -> bool override;

	/**
	 * @brief
	 *  Bind the textures of the next draws.
	 * @param[in] iTextures The 2D textures.
	 */
	void bindTextures(std::span<const shared<renderer::gpu::Texture2D>> iTextures) override;

	/**
	 * @brief
	 *  Bind the textures of the next draws.
	 * @param[in] iTextures The 2D textures.
	 * @param[in] iCubes The cube textures.
	 */
	void bindTextures(std::span<const shared<renderer::gpu::Texture2D>> iTextures,
					  std::span<const shared<renderer::gpu::TextureCube>> iCubes) override;

	/**
	 * @brief
	 *  Takes the canvas texture of the frame.
	 */
	void beginFrame() override;

	/**
	 * @brief
	 *  Releases the canvas texture of the frame; the browser shows the canvas afterwards.
	 */
	void endFrame() override;

	/**
	 * @brief
	 *  Name of the device.
	 * @return The name.
	 */
	[[nodiscard]] auto getDeviceName() const -> std::string override { return "WebGPU"; }

	/**
	 * @brief
	 *  Present mode of the canvas.
	 * @return The mode.
	 */
	[[nodiscard]] auto getPresentMode() const -> std::string override { return "fifo"; }

private:
	/**
	 * @brief
	 *  Copies the canvas of the frame and sends it to the page as a PNG (`OWL_WEBGPU_CAPTURE`).
	 */
	void captureFrame() const;

	/// Background colour of clear().
	math::vec4 m_clearColor{0.f, 0.f, 0.f, 1.f};
	/// Frames ended since the start.
	uint64_t m_frame = 0;
	/// Frame whose canvas `OWL_WEBGPU_CAPTURE` asks for, -1 for none.
	int64_t m_captureFrame = -1;
};

}// namespace owl::renderer::gpu::webgpu
