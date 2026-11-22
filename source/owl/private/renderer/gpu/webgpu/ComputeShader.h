/**
 * @file ComputeShader.h
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/webgpu.h"
#include "renderer/gpu/ComputeShader.h"

#include <cstdint>
#include <map>
#include <string>

namespace owl::renderer::gpu::webgpu {

/**
 * @brief
 *  WebGPU compute shader: the baked WGSL of its compute stage, its bindings read from the WGSL (group 0). Each
 *  dispatch records and submits a compute pass of its own, which the queue orders before the draws after it.
 */
class OWL_API ComputeShader final : public renderer::gpu::ComputeShader {
public:
	ComputeShader(const ComputeShader&) = delete;
	ComputeShader(ComputeShader&&) = delete;
	auto operator=(const ComputeShader&) -> ComputeShader& = delete;
	auto operator=(ComputeShader&&) -> ComputeShader& = delete;

	/**
	 * @brief
	 *  Loads the baked compute stage of a shader and makes its pipeline.
	 * @param[in] iShaderName Name of the shader.
	 * @param[in] iRenderer Its renderer.
	 */
	ComputeShader(const std::string& iShaderName, const std::string& iRenderer);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ComputeShader() override = default;

	/**
	 * @brief
	 *  Binds a storage buffer for the next dispatches.
	 * @param[in] iBinding Binding of the buffer in the shader.
	 * @param[in] iBuffer The buffer.
	 */
	void bindStorageBuffer(uint32_t iBinding, const shared<renderer::gpu::StorageBuffer>& iBuffer) override;

	/**
	 * @brief
	 *  Binds a texture for the next dispatches; its sampler goes to the binding after it, where WGSL puts the sampler
	 *  of a combined one.
	 * @param[in] iBinding Binding of the texture in the shader.
	 * @param[in] iTexture The texture.
	 */
	void bindTexture(uint32_t iBinding, const shared<renderer::gpu::Texture2D>& iTexture) override;

	/**
	 * @brief
	 *  Records and submits a dispatch.
	 * @param[in] iGroupsX Work groups along x.
	 * @param[in] iGroupsY Work groups along y.
	 * @param[in] iGroupsZ Work groups along z.
	 */
	void dispatch(uint32_t iGroupsX, uint32_t iGroupsY, uint32_t iGroupsZ) override;

private:
	/// Kind of a binding of the shader.
	enum struct Kind : uint8_t {
		Buffer,///< Storage buffer.
		Texture,///< Sampled texture.
		Sampler,///< Sampler.
	};

	/// Bindings of group 0 declared by the WGSL.
	std::map<uint32_t, Kind> m_bindings;
	/// Buffers bound, by binding.
	std::map<uint32_t, wgpu::Buffer> m_buffers;
	/// Texture views bound, by binding.
	std::map<uint32_t, wgpu::TextureView> m_views;
	/// Samplers bound, by binding.
	std::map<uint32_t, wgpu::Sampler> m_samplers;
	/// The pipeline, its layout derived from the shader.
	wgpu::ComputePipeline m_pipeline;
	/// Name of the shader, for the diagnostics.
	std::string m_name;
};

}// namespace owl::renderer::gpu::webgpu
