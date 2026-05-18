/**
 * @file ComputeShader.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "StorageBuffer.h"

namespace owl::renderer::gpu {

/**
 * @brief
 *  Compute shader pipeline. Separate from `Shader` because compute pipelines
 *  use `VkPipelineBindPoint::COMPUTE` (Vulkan) / a dedicated GL compute
 *  program — they don't share state with graphics pipelines and don't have
 *  vertex / fragment stages.
 *
 *  The compute shader source is a Slang file declaring a `[shader("compute")]
 *  void computeMain(uint3 gid : SV_DispatchThreadID)` entry point. SSBOs are
 *  the only resource the shader can read or write — `bindStorageBuffer(slot,
 *  ssbo)` wires them to the descriptor set before `dispatch(x, y, z)` issues
 *  the workgroup launch.
 *
 *  Typical lifecycle:
 *    - Construct with `(name, renderer)` — compiles the Slang source, builds
 *      the descriptor set layout, creates the compute pipeline.
 *    - Each frame:
 *        - `bindStorageBuffer(0, mySsbo)` ... (one per SSBO slot)
 *        - `dispatch(numGroupsX, numGroupsY, numGroupsZ)`
 *    - On shutdown the destructor releases the pipeline + descriptors.
 *
 *  Synchronisation: compute writes are NOT visible to subsequent graphics
 *  draws until a `RenderCommand::storageBufferMemoryBarrier()` is issued.
 */
class OWL_API ComputeShader {
public:
	ComputeShader() = default;

	ComputeShader(const ComputeShader&) = delete;

	ComputeShader(ComputeShader&&) = delete;

	auto operator=(const ComputeShader&) -> ComputeShader& = delete;

	auto operator=(ComputeShader&&) -> ComputeShader& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~ComputeShader();

	/**
	 * @brief
	 *  Wire an SSBO to a binding slot for the next `dispatch` call. The
	 *  binding number must match the `[[vk::binding(N)]]` annotation on the
	 *  corresponding declaration in the Slang source.
	 * @param[in] iBinding Shader binding slot.
	 * @param[in] iBuffer The SSBO to bind.
	 */
	virtual void bindStorageBuffer(uint32_t iBinding, const shared<StorageBuffer>& iBuffer) = 0;

	/**
	 * @brief
	 *  Dispatch the compute shader. Work-item count = `iGroupsX *
	 *  workgroupSize.x` etc., as declared by the shader's
	 *  `[numthreads(...)]` attribute.
	 * @param[in] iGroupsX Workgroups along X.
	 * @param[in] iGroupsY Workgroups along Y.
	 * @param[in] iGroupsZ Workgroups along Z.
	 */
	virtual void dispatch(uint32_t iGroupsX, uint32_t iGroupsY, uint32_t iGroupsZ) = 0;

	/**
	 * @brief
	 *  Factory entry. Loads the Slang source at
	 *  `engine_assets/shaders/<renderer>/slang/<name>.slang`, compiles it to
	 *  SPIR-V (compute stage), builds the backend-specific pipeline.
	 * @param[in] iShaderName Shader file name (without `.slang`).
	 * @param[in] iRenderer Owning renderer name (directory under
	 *  `engine_assets/shaders/`).
	 * @return New `ComputeShader` instance, or nullptr on compile failure.
	 */
	static auto create(const std::string& iShaderName, const std::string& iRenderer) -> shared<ComputeShader>;
};

}// namespace owl::renderer::gpu
