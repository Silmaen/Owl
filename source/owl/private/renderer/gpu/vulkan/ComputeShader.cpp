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
#include "Texture.h"
#include "internal/Descriptors.h"
#include "internal/VulkanCore.h"
#include "internal/VulkanHandler.h"
#include "internal/utils.h"
#include "renderer/Renderer.h"
#include "renderer/utils/shaderFileUtils.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wshadow")
#include <spirv_cross.hpp>
OWL_DIAG_POP

#include <format>
#include <sstream>
#include <utility>
#include <vector>

namespace owl::renderer::gpu::vulkan {

namespace {
auto loadSlangSource(const std::string& iShaderName, const std::string& iRenderer) -> std::string {
	// Slang sources live at `shaders/<renderer>/<name>.slang` (getShaderPath builds the wrong GLSL-style path).
	const auto relative = std::filesystem::path("shaders") / iRenderer / (iShaderName + std::string(".slang"));
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

struct ReflectedBindings {
	std::vector<uint32_t> storageBuffers;
	std::vector<uint32_t> sampledImages;
};

auto reflectBindings(const std::vector<uint32_t>& iSpirv) -> ReflectedBindings {
	ReflectedBindings bindings;
	if (iSpirv.empty())
		return bindings;
	const spirv_cross::Compiler compiler(iSpirv);
	const auto resources = compiler.get_shader_resources();
	const auto collect = [&compiler](const auto& iResources, std::vector<uint32_t>& oBindings) -> void {
		for (const auto& res: iResources) oBindings.push_back(compiler.get_decoration(res.id, spv::DecorationBinding));
		std::ranges::sort(oBindings);
		oBindings.erase(std::ranges::unique(oBindings).begin(), oBindings.end());
	};
	collect(resources.storage_buffers, bindings.storageBuffers);
	collect(resources.sampled_images, bindings.sampledImages);
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
	const auto compiled = renderer::utils::loadOrCompileSpirv(source, iShaderName, iRenderer, /*iForVulkan=*/true,
															  {ShaderType::Compute});
	if (!compiled.has_value()) {
		OWL_CORE_ERROR("Vulkan compute shader: Slang compilation failed for '{}'.", m_name)
		return;
	}
	const auto it = compiled->stages.find(ShaderType::Compute);
	if (it == compiled->stages.end() || it->second.empty()) {
		OWL_CORE_ERROR("Vulkan compute shader: no `computeMain` entry point in '{}'.", m_name)
		return;
	}
	const auto& spirv = it->second;
	auto reflected = reflectBindings(spirv);
	m_bindings = std::move(reflected.storageBuffers);
	m_imageBindings = std::move(reflected.sampledImages);

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

	if (auto problem = createPipeline(spirv); problem.has_value()) {
		// Stored SPIR-V the driver refuses: compile it again once, and retry.
		const auto again = renderer::utils::recompileSpirv(source, iShaderName, iRenderer, /*iForVulkan=*/true,
														   compiled->origin, *problem);
		const auto* const retry = again.has_value() && again->stages.contains(ShaderType::Compute)
										  ? &again->stages.at(ShaderType::Compute)
										  : nullptr;
		if (retry == nullptr || createPipeline(*retry).has_value()) {
			OWL_CORE_ERROR("Vulkan compute shader: {} for '{}'.", *problem, m_name)
			return;
		}
	}

	std::vector<VkDescriptorPoolSize> poolSizes;
	if (!m_bindings.empty())
		poolSizes.push_back({.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							 .descriptorCount = static_cast<uint32_t>(m_bindings.size())});
	if (!m_imageBindings.empty())
		poolSizes.push_back({.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							 .descriptorCount = static_cast<uint32_t>(m_imageBindings.size())});
	if (!poolSizes.empty())
		m_ring.init(m_descriptorLayout, poolSizes);
	m_ready = true;
}

auto ComputeShader::createPipeline(const std::vector<uint32_t>& iSpirv) -> std::optional<std::string> {
	auto* const device = internal::VulkanCore::get().getLogicalDevice();
	VkShaderModuleCreateInfo moduleInfo{};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = iSpirv.size() * sizeof(uint32_t);
	moduleInfo.pCode = iSpirv.data();
	VkShaderModule shaderModule{nullptr};
	if (const auto result = vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule); result != VK_SUCCESS)
		return std::format("vkCreateShaderModule failed ({})", internal::resultString(result));

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
		m_pipeline = nullptr;
		return std::format("vkCreateComputePipelines failed ({})", internal::resultString(result));
	}
	return std::nullopt;
}

ComputeShader::~ComputeShader() {
	m_bound.clear();
	m_boundTextures.clear();
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		m_ring.reset();
		return;
	}
	m_ring.release();
	internal::VulkanHandler::get().deferRelease([device = internal::VulkanCore::get().getLogicalDevice(),
												 pipeline = m_pipeline, layout = m_layout,
												 setLayout = m_descriptorLayout]() -> void {
		if (pipeline != nullptr)
			vkDestroyPipeline(device, pipeline, nullptr);
		if (layout != nullptr)
			vkDestroyPipelineLayout(device, layout, nullptr);
		if (setLayout != nullptr)
			vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
	});
}

void ComputeShader::buildDescriptorSetLayout(const std::vector<uint32_t>& iBindings) {
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	layoutBindings.reserve(iBindings.size() + m_imageBindings.size());
	const auto add = [&layoutBindings](const uint32_t iBinding, const VkDescriptorType iType) -> void {
		layoutBindings.push_back({.binding = iBinding,
								  .descriptorType = iType,
								  .descriptorCount = 1,
								  .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
								  .pImmutableSamplers = nullptr});
	};
	for (const auto binding: iBindings) add(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	for (const auto binding: m_imageBindings) add(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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
	if (!m_ready || !iBuffer)
		return;
	const auto* vkBuf = dynamic_cast<const StorageBuffer*>(iBuffer.get());
	if (vkBuf == nullptr) {
		OWL_CORE_WARN("Vulkan compute shader: bindStorageBuffer with non-Vulkan SSBO on '{}'.", m_name)
		return;
	}
	for (auto& [binding, buffer]: m_bound) {
		if (binding == iBinding) {
			buffer = iBuffer;
			return;
		}
	}
	m_bound.emplace_back(iBinding, iBuffer);
}

void ComputeShader::bindTexture(const uint32_t iBinding, const shared<renderer::gpu::Texture2D>& iTexture) {
	if (!m_ready || !iTexture)
		return;
	if (dynamic_cast<const Texture2D*>(iTexture.get()) == nullptr) {
		OWL_CORE_WARN("Vulkan compute shader: bindTexture with a non-Vulkan texture on '{}'.", m_name)
		return;
	}
	for (auto& [binding, texture]: m_boundTextures) {
		if (binding == iBinding) {
			texture = iTexture;
			return;
		}
	}
	m_boundTextures.emplace_back(iBinding, iTexture);
}

void ComputeShader::writeDescriptors(VkDescriptorSet iSet) {
	std::vector<VkDescriptorBufferInfo> bufferInfos;
	bufferInfos.reserve(m_bound.size());
	std::vector<VkDescriptorImageInfo> imageInfos;
	imageInfos.reserve(m_boundTextures.size());
	std::vector<VkWriteDescriptorSet> writes;
	writes.reserve(m_bound.size() + m_boundTextures.size());
	for (const auto& [binding, buffer]: m_bound) {
		const auto view = static_cast<StorageBuffer*>(buffer.get())->resolve();
		if (view.buffer == nullptr)
			continue;
		const auto& info = bufferInfos.emplace_back(
				VkDescriptorBufferInfo{.buffer = view.buffer, .offset = view.offset, .range = view.range});
		writes.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						  .pNext = nullptr,
						  .dstSet = iSet,
						  .dstBinding = binding,
						  .dstArrayElement = 0,
						  .descriptorCount = 1,
						  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
						  .pImageInfo = nullptr,
						  .pBufferInfo = &info,
						  .pTexelBufferView = nullptr});
	}
	auto& textures = internal::Descriptors::get();
	for (const auto& [binding, texture]: m_boundTextures) {
		const uint32_t id = static_cast<const Texture2D*>(texture.get())->getTextureId();
		if (!textures.isTextureRegistered(id))
			continue;
		const auto& data = textures.getTextureData(id);
		if (data.textureSampler == nullptr || data.textureImageView == nullptr)
			continue;
		const auto& info =
				imageInfos.emplace_back(VkDescriptorImageInfo{.sampler = data.textureSampler,
															  .imageView = data.textureImageView,
															  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
		writes.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						  .pNext = nullptr,
						  .dstSet = iSet,
						  .dstBinding = binding,
						  .dstArrayElement = 0,
						  .descriptorCount = 1,
						  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						  .pImageInfo = &info,
						  .pBufferInfo = nullptr,
						  .pTexelBufferView = nullptr});
	}
	vkUpdateDescriptorSets(internal::VulkanCore::get().getLogicalDevice(), static_cast<uint32_t>(writes.size()),
						   writes.data(), 0, nullptr);
}

void ComputeShader::dispatch(const uint32_t iGroupsX, const uint32_t iGroupsY, const uint32_t iGroupsZ) {
	if (!m_ready || iGroupsX == 0 || iGroupsY == 0 || iGroupsZ == 0)
		return;
	auto& vkh = internal::VulkanHandler::get();
	// Recorded in the frame command buffer (a frame starts if none is recording); a read-back flushes it.
	if (vkh.getCurrentCommandBuffer() == nullptr)
		return;
	const auto record = [this, &vkh, iGroupsX, iGroupsY, iGroupsZ](VkCommandBuffer iCmd) -> void {
		VkDescriptorSet set = nullptr;
		if (!m_bindings.empty() || !m_imageBindings.empty()) {
			set = m_ring.acquire(vkh.getCurrentFrameIndex(), vkh.getFrameSerial());
			if (set == nullptr)
				return;
			writeDescriptors(set);
		}
		vkCmdBindPipeline(iCmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
		if (set != nullptr)
			vkCmdBindDescriptorSets(iCmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &set, 0, nullptr);
		vkCmdDispatch(iCmd, iGroupsX, iGroupsY, iGroupsZ);
	};
	vkh.recordTransfer(record);
}

}// namespace owl::renderer::gpu::vulkan
