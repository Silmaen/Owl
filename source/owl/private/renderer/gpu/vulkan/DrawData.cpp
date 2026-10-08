/**
 * @file DrawData.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "DrawData.h"

#include "internal/VulkanHandler.h"
#include "renderer/Renderer.h"

#include <format>
#include <unordered_set>

namespace owl::renderer::gpu::vulkan {

namespace {
// Every initialised Vulkan draw, so that a shader reload rebuilds the pipelines built from it.
auto liveDrawData() -> std::unordered_set<DrawData*>& {
	static std::unordered_set<DrawData*> s_live;
	return s_live;
}
}// namespace

DrawData::~DrawData() { liveDrawData().erase(this); }

void DrawData::init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
					const std::string& iShaderName, const PipelineState& iState) {
	m_pipelineState = iState;
	m_shaderName = iShaderName;
	m_renderer = iRenderer;
	setShader(iShaderName, iRenderer);
	if (iLayout.getStride() != 0) {
		mp_vertexBuffer = mkShared<VertexBuffer>(iLayout.getStride() * iIndices.size());
		mp_vertexBuffer->setLayout(iLayout);
		mp_indexBuffer = mkShared<IndexBuffer>(iIndices.data(), iIndices.size());
	}
	buildPipeline(nullptr);
}

void DrawData::initInstanced(const BufferLayout& iVertexLayout, const BufferLayout& iInstanceLayout,
							 const uint32_t iVertexCapacity, const uint32_t iInstanceCapacity,
							 const std::string& iRenderer, std::vector<uint32_t>& iIndices,
							 const std::string& iShaderName, const PipelineState& iState) {
	m_pipelineState = iState;
	if (iVertexLayout.getStride() == 0 || iInstanceLayout.getStride() == 0)
		return;
	m_shaderName = iShaderName;
	m_renderer = iRenderer;
	setShader(iShaderName, iRenderer);

	mp_vertexBuffer = mkShared<VertexBuffer>(iVertexLayout.getStride() * iVertexCapacity);
	mp_vertexBuffer->setLayout(iVertexLayout);
	mp_instanceBuffer = mkShared<VertexBuffer>(iInstanceLayout.getStride() * iInstanceCapacity);
	mp_instanceBuffer->setLayout(iInstanceLayout);
	mp_indexBuffer = mkShared<IndexBuffer>(iIndices.data(), iIndices.size());
	buildPipeline(nullptr);
}

void DrawData::buildPipeline(VkDescriptorSetLayout iSetLayout) {
	if (!mp_shader)
		return;
	auto& vkh = internal::VulkanHandler::get();
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = mp_shader->getStagesInfo();

	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	if (mp_vertexBuffer && mp_instanceBuffer) {
		bindings = {mp_vertexBuffer->getBindingDescription(/*iBinding=*/0, /*iPerInstance=*/false),
					mp_instanceBuffer->getBindingDescription(/*iBinding=*/1, /*iPerInstance=*/true)};
		attributes = mp_vertexBuffer->getAttributeDescriptions(/*iBinding=*/0, /*iStartLocation=*/0);
		const auto instanceAttribs = mp_instanceBuffer->getAttributeDescriptions(
				/*iBinding=*/1, /*iStartLocation=*/static_cast<uint32_t>(attributes.size()));
		attributes.insert(attributes.end(), instanceAttribs.begin(), instanceAttribs.end());
	} else if (mp_vertexBuffer) {
		bindings = {mp_vertexBuffer->getBindingDescription()};
		attributes = mp_vertexBuffer->getAttributeDescriptions();
	}
	const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size()),
			.pVertexBindingDescriptions = bindings.empty() ? nullptr : bindings.data(),
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size()),
			.pVertexAttributeDescriptions = attributes.empty() ? nullptr : attributes.data(),
	};
	if (m_pipelineId >= 0)
		vkh.popPipeline(m_pipelineId);
	// The generation keeps a reloaded shader out of the deduplication of the pipelines built from its old binaries.
	const auto pipelineName = mp_shader->getGeneration() == 0
									  ? mp_shader->getName()
									  : std::format("{}@{}", mp_shader->getName(), mp_shader->getGeneration());
	m_pipelineId = vkh.pushPipeline(pipelineName, shaderStages, vertexInputInfo, m_pipelineState, iSetLayout);
	const auto& vkc = internal::VulkanCore::get();
	for (const auto& stage: shaderStages) vkDestroyShaderModule(vkc.getLogicalDevice(), stage.module, nullptr);
	if (m_pipelineId < 0) {
		OWL_CORE_WARN("Vulkan shader: Failed to register pipeline {}.", mp_shader->getName())
		return;
	}
	liveDrawData().insert(this);
}

void DrawData::rebuildPipelines(const Shader& iShader) {
	auto& vkh = internal::VulkanHandler::get();
	for (auto* draw: liveDrawData()) {
		if (draw->mp_shader.get() != &iShader || draw->m_pipelineId < 0)
			continue;
		draw->buildPipeline(vkh.getPipeline(draw->m_pipelineId).setLayout);
	}
}

void DrawData::setShader(const std::string& iShaderName, const std::string& iRenderer) {
	auto& shLib = Renderer::getShaderLibrary();
	const auto baseName = Shader::composeName({.name = iShaderName, .renderer = iRenderer});
	if (!shLib.exists(baseName))
		shLib.load(baseName);
	mp_shader = static_pointer_cast<Shader>(shLib.get(baseName));
}

void DrawData::bind() const {
	if (m_pipelineId < 0)
		return;
	auto& vkh = internal::VulkanHandler::get();
	vkh.bindPipeline(m_pipelineId, m_pipelineState);
	if (mp_vertexBuffer)
		mp_vertexBuffer->bind();
	if (mp_instanceBuffer)
		mp_instanceBuffer->bindAtBinding(1);
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

void DrawData::setInstanceData(const void* iData, const uint32_t iSize) {
	if (m_pipelineId < 0)
		return;
	if (mp_instanceBuffer)
		mp_instanceBuffer->setData(iData, iSize);
}

auto DrawData::getIndexCount() const -> uint32_t {
	if (m_pipelineId < 0)
		return 0;
	if (mp_indexBuffer)
		return mp_indexBuffer->getCount();
	return 0;
}

}// namespace owl::renderer::gpu::vulkan
