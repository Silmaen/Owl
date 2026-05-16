/**
 * @file ComputeShader.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/ComputeShader.h"

namespace owl::renderer::gpu::opengl {

/**
 * @brief
 *  OpenGL-backed compute shader. Holds a GL program built from a
 *  Slang-compiled SPIR-V compute stage. SSBOs are already bound globally by
 *  `StorageBuffer::bind` (via `glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
 *  slot, …)`), so `bindStorageBuffer` here just delegates to the SSBO's
 *  own `bind()`.
 */
class OWL_API ComputeShader final : public renderer::gpu::ComputeShader {
public:
	ComputeShader(const ComputeShader&) = delete;

	ComputeShader(ComputeShader&&) = delete;

	auto operator=(const ComputeShader&) -> ComputeShader& = delete;

	auto operator=(ComputeShader&&) -> ComputeShader& = delete;

	/**
	 * @brief
	 *  Compile the Slang compute shader at
	 *  `engine_assets/shaders/<iRenderer>/slang/<iShaderName>.slang` and
	 *  link it into a GL program.
	 * @param[in] iShaderName Shader file name (without extension).
	 * @param[in] iRenderer Owning renderer name (directory).
	 */
	ComputeShader(const std::string& iShaderName, const std::string& iRenderer);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ComputeShader() override;

	/**
	 * @brief
	 *  Bind an SSBO to its declared slot (delegates to the SSBO).
	 * @param[in] iBinding Shader binding slot.
	 * @param[in] iBuffer The SSBO to bind.
	 */
	void bindStorageBuffer(uint32_t iBinding, const shared<renderer::gpu::StorageBuffer>& iBuffer) override;

	/**
	 * @brief
	 *  `glUseProgram(prog)` + `glDispatchCompute(x, y, z)`.
	 * @param[in] iGroupsX Workgroups along X.
	 * @param[in] iGroupsY Workgroups along Y.
	 * @param[in] iGroupsZ Workgroups along Z.
	 */
	void dispatch(uint32_t iGroupsX, uint32_t iGroupsY, uint32_t iGroupsZ) override;

private:
	/// GL program handle.
	uint32_t m_programId = 0;
	/// Shader name (for diagnostics).
	std::string m_name;
};

}// namespace owl::renderer::gpu::opengl
