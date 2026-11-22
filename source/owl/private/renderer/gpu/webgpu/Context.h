/**
 * @file Context.h
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "PbrTextures.h"
#include "core/external/webgpu.h"
#include "math/vectors.h"
#include "renderer/gpu/RendererDescriptors.h"

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::renderer::gpu::webgpu {

class DrawData;
class Framebuffer;

/**
 * @brief
 *  Where the draws go: the canvas, or the attachments of a framebuffer.
 */
struct Target {
	/// Colour attachments; a null view leaves its location out.
	std::vector<wgpu::TextureView> colors;
	/// Formats of colors.
	std::vector<wgpu::TextureFormat> formats;
	/// Depth attachment, or null.
	wgpu::TextureView depth;
};

/**
 * @brief
 *  Tells whether a format holds integers, which a pipeline does not blend and a colour clear leaves.
 * @param[in] iFormat The format.
 * @return True for the integer formats.
 */
auto isIntegerFormat(wgpu::TextureFormat iFormat) -> bool;

/**
 * @brief
 *  Tells whether a pipeline may blend into a format: not the integers, nor the 32-bit floats (a feature the page does
 *  not ask for).
 * @param[in] iFormat The format.
 * @return True for a blendable format.
 */
auto isBlendableFormat(wgpu::TextureFormat iFormat) -> bool;

/// Texture slots of a draw: a WebGPU stage samples sixteen textures (`OWL_WEBGPU_TEXTURE_SLOTS` of the shaders).
constexpr uint32_t g_textureSlots = 16;
/// Cube slots of a draw (`OWL_WEBGPU_CUBE_SLOTS` of the shaders), bindings 32 to 39 of group 1.
constexpr uint32_t g_cubeSlots = 8;
/// First binding of the cubes in group 1.
constexpr uint32_t g_firstCubeBinding = 32;
/// Binding of the sampler the cubes share in group 1.
constexpr uint32_t g_cubeSamplerBinding = g_firstCubeBinding + g_cubeSlots;

/**
 * @brief
 *  The bindings a renderer declared (`RendererDescriptors::declare`): its buffers in bind group 0, at their binding
 *  numbers, and its textures in bind group 1, as its shaders declare them under `BACKEND_WEBGPU`:
 *  - a texture table: textures 0 to 15, samplers 16 to 31;
 *  - cubes alone (`OWL_WEBGPU_CUBE_TABLE`): cubes 32 to 39, their sampler 40;
 *  - 2D textures and cubes, the 3D renderer: the layout of `PbrTextures`.
 */
struct Block {
	/// Renderer name.
	std::string name;
	/// Buffer bindings of group 0.
	std::vector<BindingDecl> buffers;
	/// Layout of group 0.
	wgpu::BindGroupLayout bufferLayout;
	/// Layout of group 1.
	wgpu::BindGroupLayout textureLayout;
	/// What group 1 holds.
	enum struct Textures : uint8_t {
		Table,///< The texture table.
		Cubes,///< The cube table.
		Pbr,///< The table, the shadow arrays and the probe array of `PbrTextures`.
	};
	/// What group 1 holds.
	Textures textures = Textures::Table;
	/// Layout of the pipelines of the renderer.
	wgpu::PipelineLayout pipelineLayout;
};

/**
 * @brief
 *  The WebGPU objects of the page and the bindings of the next draw. Every draw records and submits a render pass of
 *  its own: the queue orders it after the buffer writes made before it, so each draw sees its own data.
 */
struct Context {
	/// Instance the surface was made from, kept alive as long as the surface.
	wgpu::Instance instance;
	/// Device made by the page before main (`Module.preinitializedWebGPUDevice`).
	wgpu::Device device;
	/// Queue of the device.
	wgpu::Queue queue;
	/// Surface of the canvas.
	wgpu::Surface surface;
	/// Format the surface is configured with.
	wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
	/// Size of the surface in pixels.
	uint32_t width = 0;
	/// Size of the surface in pixels.
	uint32_t height = 0;
	/// Texture of the canvas for the current frame.
	wgpu::Texture frameTexture;
	/// View of frameTexture.
	wgpu::TextureView frameView;
	/// Depth attachment of the canvas.
	wgpu::Texture depthTexture;
	/// View of depthTexture.
	wgpu::TextureView depthView;
	/// The declared renderer blocks, by name.
	std::unordered_map<std::string, Block> blocks;
	/// The active block (`RendererDescriptors::ScopedActive`), or nullptr.
	const Block* activeBlock = nullptr;
	/// The framebuffer the draws go to, or nullptr for the canvas.
	Framebuffer* framebuffer = nullptr;
	/// Buffers bound for the next draws, by renderer block then by binding number of group 0.
	std::unordered_map<std::string, std::unordered_map<uint32_t, wgpu::Buffer>> buffers;
	/// Texture views bound for the next draws, by slot.
	std::vector<wgpu::TextureView> textureViews;
	/// Samplers of textureViews.
	std::vector<wgpu::Sampler> samplers;
	/// Buffer bound where a draw has none (zeros).
	wgpu::Buffer emptyBuffer;
	/// Texture bound where a draw has none (white).
	wgpu::Texture whiteTexture;
	/// View of whiteTexture.
	wgpu::TextureView whiteView;
	/// Sampler of whiteTexture.
	wgpu::Sampler defaultSampler;
	/// Cube views bound for the next draws, by slot.
	std::vector<wgpu::TextureView> cubeViews;
	/// Cube bound where a draw has none (white).
	wgpu::Texture whiteCube;
	/// Cube view of whiteCube.
	wgpu::TextureView whiteCubeView;
	/// Sampler the cubes share (linear, clamped).
	wgpu::Sampler cubeSampler;
	/// The 2D textures of the last `bindTextures`, read by the 3D layout.
	std::vector<shared<renderer::gpu::Texture2D>> boundTextures;
	/// The cubes of the last `bindTextures`, read by the 3D layout.
	std::vector<shared<renderer::gpu::TextureCube>> boundCubes;
	/// The textures of the 3D layout.
	PbrTextures pbr;
	/// Frames begun.
	uint64_t frame = 0;

	/// Depth format of every attachment.
	static constexpr wgpu::TextureFormat depthFormat = wgpu::TextureFormat::Depth32Float;

	/**
	 * @brief
	 *  The context of the page.
	 * @return The only context.
	 */
	static auto get() -> Context&;

	/**
	 * @brief
	 *  Makes the resources bound where a draw has none.
	 */
	void initDefaults();

	/**
	 * @brief
	 *  Releases every WebGPU object, before the runtime of emdawnwebgpu shuts down.
	 */
	void release();

	/**
	 * @brief
	 *  Configures the surface and its depth attachment for a size, again when the canvas changed.
	 * @param[in] iWidth Width in pixels.
	 * @param[in] iHeight Height in pixels.
	 */
	void configure(uint32_t iWidth, uint32_t iHeight);

	/**
	 * @brief
	 *  Declares the bindings of a renderer.
	 * @param[in] iRenderer Name of the renderer.
	 * @param[in] iBindings Its bindings.
	 */
	void declare(const std::string& iRenderer, std::span<const BindingDecl> iBindings);

	/**
	 * @brief
	 *  Binds a buffer for the next draws of a renderer block.
	 * @param[in] iRenderer The block, empty for the active one.
	 * @param[in] iBinding Binding of group 0.
	 * @param[in] iBuffer The buffer.
	 */
	void bindBuffer(const std::string& iRenderer, uint32_t iBinding, const wgpu::Buffer& iBuffer);

	/**
	 * @brief
	 *  The attachments the draws go to now.
	 * @return The canvas, or the bound framebuffer.
	 */
	[[nodiscard]] auto getTarget() const -> Target;

	/**
	 * @brief
	 *  Clears the colour and the depth of the target.
	 * @param[in] iColor Background colour.
	 */
	void clear(const math::vec4& iColor);

	/**
	 * @brief
	 *  Clears one view.
	 * @param[in] iView The view.
	 * @param[in] iColor The value, or the depth in its x.
	 * @param[in] iDepth True for a depth view.
	 */
	void clearView(const wgpu::TextureView& iView, const wgpu::Color& iColor, bool iDepth) const;

	/**
	 * @brief
	 *  Records and submits a draw on the target.
	 * @param[in] iData The draw.
	 * @param[in] iIndexCount Indices per instance.
	 * @param[in] iInstanceCount Instances.
	 * @param[in] iIndirect Buffer of a `DrawIndexedIndirectCommand` to draw instead of the counts, or null.
	 * @param[in] iIndirectOffset Offset of the command in iIndirect.
	 */
	void draw(const DrawData& iData, uint32_t iIndexCount, uint32_t iInstanceCount,
			  const wgpu::Buffer& iIndirect = nullptr, uint64_t iIndirectOffset = 0);
};

}// namespace owl::renderer::gpu::webgpu
