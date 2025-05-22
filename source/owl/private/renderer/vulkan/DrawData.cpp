/**
 * @file DrawData.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "DrawData.h"

#include "internal/VulkanHandler.h"
#include "renderer/Renderer.h"

namespace owl::renderer::vulkan {

DrawData::~DrawData() = default;

void DrawData::init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
					const std::string& iShaderName) {
	m_shaderName = iShaderName;
	m_renderer = iRenderer;
	setShader(iShaderName, iRenderer);
	if (iLayout.getStride() != 0) {
		mp_vertexBuffer = mkShared<VertexBuffer>(iLayout.getStride() * iIndices.size());
		mp_vertexBuffer->setLayout(iLayout);
		mp_indexBuffer = mkShared<IndexBuffer>(iIndices.data(), iIndices.size());
	}
	auto& vkh = internal::VulkanHandler::get();
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = mp_shader->getStagesInfo();

	VkVertexInputBindingDescription bindingDescription;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	if (mp_vertexBuffer) {
		attributeDescriptions = mp_vertexBuffer->getAttributeDescriptions();
		bindingDescription = mp_vertexBuffer->getBindingDescription();
	}
	const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.vertexBindingDescriptionCount = mp_vertexBuffer ? 1u : 0u,
			.pVertexBindingDescriptions = mp_vertexBuffer ? &bindingDescription : nullptr,
			.vertexAttributeDescriptionCount =
					mp_vertexBuffer ? static_cast<uint32_t>(attributeDescriptions.size()) : 0,
			.pVertexAttributeDescriptions = mp_vertexBuffer ? attributeDescriptions.data() : nullptr,
	};
	if (m_pipelineId >= 0)
		vkh.popPipeline(m_pipelineId);
	m_pipelineId = vkh.pushPipeline(mp_shader->getName(), shaderStages, vertexInputInfo);
	const auto& vkc = internal::VulkanCore::get();

	for (const auto& stage: shaderStages) vkDestroyShaderModule(vkc.getLogicalDevice(), stage.module, nullptr);
	if (m_pipelineId < 0) {
		OWL_CORE_WARN("Vulkan shader: Failed to register pipeline {}.", mp_shader->getName())
	}
}

void DrawData::setShader(const std::string& iShaderName, const std::string& iRenderer) {
	auto& shLib = Renderer::getShaderLibrary();
	const auto baseName = Shader::composeName({.name=iShaderName, .renderer=iRenderer});
	if (!shLib.exists(baseName))
		shLib.load(baseName);
	mp_shader = static_pointer_cast<Shader>(shLib.get(baseName));
}

void DrawData::bind() const {
	if (m_pipelineId < 0)
		return;
	auto& vkh = internal::VulkanHandler::get();
	vkh.bindPipeline(m_pipelineId);
	if (mp_vertexBuffer)
		mp_vertexBuffer->bind();
	if (mp_indexBuffer)
		mp_indexBuffer->bind();
}

void DrawData::unbind() const {}

void DrawData::setVertexData(const void* iData, const uint32_t iSize) {
	if (m_pipelineId < 0)
		return;
	if (mp_vertexBuffer)
		mp_vertexBuffer->setData(iData, iSize);
}

auto DrawData::getIndexCount() const -> uint32_t {
	if (m_pipelineId < 0)
		return 0;
	if (mp_indexBuffer)
		return mp_indexBuffer->getCount();
	return 0;
}

}// namespace owl::renderer::vulkan
