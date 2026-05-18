/**
 * @file ComputeShader.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/ComputeShader.h"
#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan {

/**
 * @brief
 *  Vulkan-backed compute shader. Owns its own `VkPipeline` (compute bind
 *  point), `VkPipelineLayout`, `VkDescriptorSetLayout`, `VkDescriptorPool`,
 *  and `VkDescriptorSet` — self-contained so each compute pass can manage
 *  its own SSBOs without colliding with the central `Descriptors`
 *  singleton used by the graphics pipelines.
 *
 *  The Slang shader's SSBO bindings are auto-discovered from SPIR-V
 *  reflection at construction time; `bindStorageBuffer(slot, ssbo)` updates
 *  the descriptor write that maps the slot to the SSBO's `VkBuffer`.
 *  `dispatch()` records `vkCmdBindPipeline` (COMPUTE) +
 *  `vkCmdBindDescriptorSets` + `vkCmdDispatch` on the active command
 *  buffer.
 */
class OWL_API ComputeShader final : public renderer::gpu::ComputeShader {
public:
	ComputeShader(const ComputeShader&) = delete;

	ComputeShader(ComputeShader&&) = delete;

	auto operator=(const ComputeShader&) -> ComputeShader& = delete;

	auto operator=(ComputeShader&&) -> ComputeShader& = delete;

	/**
	 * @brief
	 *  Compile the Slang compute shader, build the compute pipeline.
	 * @param[in] iShaderName Shader file name (without extension).
	 * @param[in] iRenderer Owning renderer name (directory under
	 *  `engine_assets/shaders/`).
	 */
	ComputeShader(const std::string& iShaderName, const std::string& iRenderer);

	/**
	 * @brief
	 *  Destructor.
	 */
	~ComputeShader() override;

	/**
	 * @brief
	 *  Record a `VkWriteDescriptorSet` mapping `iBinding` to the SSBO.
	 *  Repeated calls for the same binding overwrite the previous mapping.
	 * @param[in] iBinding Shader binding slot.
	 * @param[in] iBuffer The SSBO to bind.
	 */
	void bindStorageBuffer(uint32_t iBinding, const shared<renderer::gpu::StorageBuffer>& iBuffer) override;

	/**
	 * @brief
	 *  Record the compute dispatch on the active command buffer.
	 * @param[in] iGroupsX Workgroups along X.
	 * @param[in] iGroupsY Workgroups along Y.
	 * @param[in] iGroupsZ Workgroups along Z.
	 */
	void dispatch(uint32_t iGroupsX, uint32_t iGroupsY, uint32_t iGroupsZ) override;

private:
	/**
	 * @brief
	 *  Build the descriptor set layout from the bindings declared in the
	 *  Slang shader (auto-discovered via SPIR-V reflection).
	 * @param[in] iBindings Set of SSBO binding slots used by the shader.
	 */
	void buildDescriptorSetLayout(const std::vector<uint32_t>& iBindings);

	/// SSBO binding slots declared by the compute shader (from reflection).
	std::vector<uint32_t> m_bindings;
	/// Compute pipeline.
	VkPipeline m_pipeline{nullptr};
	/// Compute pipeline layout.
	VkPipelineLayout m_layout{nullptr};
	/// Descriptor set layout for SSBOs.
	VkDescriptorSetLayout m_descriptorLayout{nullptr};
	/// Descriptor pool owning the descriptor set.
	VkDescriptorPool m_descriptorPool{nullptr};
	/// Descriptor set bound at dispatch time.
	VkDescriptorSet m_descriptorSet{nullptr};
	/// Shader name (for diagnostics).
	std::string m_name;
	/// True when construction succeeded.
	bool m_ready = false;
};

}// namespace owl::renderer::gpu::vulkan
