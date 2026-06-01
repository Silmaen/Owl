/**
 * @file ComputeShader.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "ComputeShader.h"

#include "StorageBuffer.h"
#include "internal/VulkanCore.h"
#include "internal/VulkanHandler.h"
#include "renderer/Renderer.h"
#include "renderer/utils/shaderFileUtils.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wshadow")
#include <spirv_cross.hpp>
OWL_DIAG_POP

namespace owl::renderer::gpu::vulkan {

namespace {
auto loadSlangSource(const std::string& iShaderName, const std::string& iRenderer) -> std::string {
	// Slang sources live at `shaders/<renderer>/slang/<name>.slang` (getShaderPath builds the wrong GLSL-style path).
	const auto relative =
			std::filesystem::path("shaders") / iRenderer / "slang" / (iShaderName + std::string(".slang"));
	const auto sourcePath =
			Renderer::getTextureLibrary().find(relative.generic_string()).value_or(std::filesystem::path{});
	if (sourcePath.empty() || !exists(sourcePath)) {
		OWL_CORE_ERROR("Vulkan compute shader: source not found for '{}' / '{}'.", iRenderer, iShaderName)
		return {};
	}
	std::ifstream in(sourcePath, std::ios::binary);
	if (!in.is_open()) {
		OWL_CORE_ERROR("Vulkan compute shader: cannot open '{}'.", sourcePath.string())
		return {};
	}
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

auto reflectSsboBindings(const std::vector<uint32_t>& iSpirv) -> std::vector<uint32_t> {
	std::vector<uint32_t> bindings;
	if (iSpirv.empty())
		return bindings;
	const spirv_cross::Compiler compiler(iSpirv);
	const auto resources = compiler.get_shader_resources();
	bindings.reserve(resources.storage_buffers.size());
	for (const auto& res: resources.storage_buffers) {
		const auto binding = compiler.get_decoration(res.id, spv::DecorationBinding);
		bindings.push_back(binding);
	}
	std::ranges::sort(bindings);
	bindings.erase(std::ranges::unique(bindings).begin(), bindings.end());
	return bindings;
}
}// namespace

ComputeShader::ComputeShader(const std::string& iShaderName, const std::string& iRenderer)
	: m_name{iShaderName + "@" + iRenderer} {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan compute shader: VulkanHandler not running for '{}'.", m_name)
		return;
	}
	const std::string source = loadSlangSource(iShaderName, iRenderer);
	if (source.empty())
		return;
	const auto compiled = renderer::utils::compileSlangToSpirv(source, iShaderName, /*iForVulkan=*/true);
	if (!compiled.success) {
		OWL_CORE_ERROR("Vulkan compute shader: Slang compilation failed for '{}'.", m_name)
		return;
	}
	const auto it = compiled.spirvData.find(ShaderType::Compute);
	if (it == compiled.spirvData.end() || it->second.empty()) {
		OWL_CORE_ERROR("Vulkan compute shader: no `computeMain` entry point in '{}'.", m_name)
		return;
	}
	const auto& spirv = it->second;
	m_bindings = reflectSsboBindings(spirv);

	auto* const device = internal::VulkanCore::get().getLogicalDevice();
	buildDescriptorSetLayout(m_bindings);
	if (m_descriptorLayout == nullptr)
		return;

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &m_descriptorLayout;
	if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan compute shader: failed to create pipeline layout for '{}'.", m_name)
		return;
	}

	VkShaderModuleCreateInfo moduleInfo{};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = spirv.size() * sizeof(uint32_t);
	moduleInfo.pCode = spirv.data();
	VkShaderModule shaderModule{nullptr};
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan compute shader: failed to create shader module for '{}'.", m_name)
		return;
	}

	// Slang collapses every entry point name to the SPIR-V canonical `main`.
	const VkPipelineShaderStageCreateInfo stageInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
													.pNext = nullptr,
													.flags = {},
													.stage = VK_SHADER_STAGE_COMPUTE_BIT,
													.module = shaderModule,
													.pName = "main",
													.pSpecializationInfo = nullptr};
	const VkComputePipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
												   .pNext = nullptr,
												   .flags = {},
												   .stage = stageInfo,
												   .layout = m_layout,
												   .basePipelineHandle = nullptr,
												   .basePipelineIndex = 0};
	const auto result = vkCreateComputePipelines(device, nullptr, 1, &pipelineInfo, nullptr, &m_pipeline);
	vkDestroyShaderModule(device, shaderModule, nullptr);
	if (result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan compute shader: vkCreateComputePipelines failed for '{}' (code {}).", m_name,
					   static_cast<int>(result))
		return;
	}

	if (!m_bindings.empty()) {
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSize.descriptorCount = static_cast<uint32_t>(m_bindings.size());
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan compute shader: failed to create descriptor pool for '{}'.", m_name)
			return;
		}
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_descriptorLayout;
		if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan compute shader: failed to allocate descriptor set for '{}'.", m_name)
			return;
		}
	}
	m_ready = true;
}

ComputeShader::~ComputeShader() {
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		m_pipeline = nullptr;
		m_layout = nullptr;
		m_descriptorLayout = nullptr;
		m_descriptorPool = nullptr;
		m_descriptorSet = nullptr;
		return;
	}
	auto* const device = internal::VulkanCore::get().getLogicalDevice();
	if (m_pipeline != nullptr)
		vkDestroyPipeline(device, m_pipeline, nullptr);
	if (m_layout != nullptr)
		vkDestroyPipelineLayout(device, m_layout, nullptr);
	if (m_descriptorPool != nullptr)
		vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
	if (m_descriptorLayout != nullptr)
		vkDestroyDescriptorSetLayout(device, m_descriptorLayout, nullptr);
	m_pipeline = nullptr;
	m_layout = nullptr;
	m_descriptorPool = nullptr;
	m_descriptorLayout = nullptr;
	m_descriptorSet = nullptr;
}

void ComputeShader::buildDescriptorSetLayout(const std::vector<uint32_t>& iBindings) {
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	layoutBindings.reserve(iBindings.size());
	for (const auto binding: iBindings) {
		VkDescriptorSetLayoutBinding b{};
		b.binding = binding;
		b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		b.descriptorCount = 1;
		b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		layoutBindings.push_back(b);
	}
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	info.pBindings = layoutBindings.empty() ? nullptr : layoutBindings.data();
	auto* const device = internal::VulkanCore::get().getLogicalDevice();
	if (vkCreateDescriptorSetLayout(device, &info, nullptr, &m_descriptorLayout) != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan compute shader: failed to create descriptor set layout for '{}'.", m_name)
		m_descriptorLayout = nullptr;
	}
}

void ComputeShader::bindStorageBuffer(const uint32_t iBinding, const shared<renderer::gpu::StorageBuffer>& iBuffer) {
	if (!m_ready || m_descriptorSet == nullptr || !iBuffer)
		return;
	const auto* vkBuf = dynamic_cast<const StorageBuffer*>(iBuffer.get());
	if (vkBuf == nullptr || vkBuf->getHandle() == nullptr) {
		OWL_CORE_WARN("Vulkan compute shader: bindStorageBuffer with non-Vulkan / empty SSBO on '{}'.", m_name)
		return;
	}
	VkDescriptorBufferInfo bufInfo{};
	bufInfo.buffer = vkBuf->getHandle();
	bufInfo.offset = 0;
	bufInfo.range = vkBuf->getSize();
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_descriptorSet;
	write.dstBinding = iBinding;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &bufInfo;
	auto* const device = internal::VulkanCore::get().getLogicalDevice();
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void ComputeShader::dispatch(const uint32_t iGroupsX, const uint32_t iGroupsY, const uint32_t iGroupsZ) {
	if (!m_ready || iGroupsX == 0 || iGroupsY == 0 || iGroupsZ == 0)
		return;
	// One-shot command buffer: callers may run before any batch is open, and `endSingleTimeCommands`'s queue wait acts as the barrier.
	const auto& core = internal::VulkanCore::get();
	auto* const cmd = core.beginSingleTimeCommands();
	if (cmd == nullptr) {
		OWL_CORE_WARN("Vulkan compute shader: failed to allocate one-shot command buffer for '{}'.", m_name)
		return;
	}
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
	if (m_descriptorSet != nullptr)
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &m_descriptorSet, 0, nullptr);
	vkCmdDispatch(cmd, iGroupsX, iGroupsY, iGroupsZ);
	core.endSingleTimeCommands(cmd);
}

}// namespace owl::renderer::gpu::vulkan
