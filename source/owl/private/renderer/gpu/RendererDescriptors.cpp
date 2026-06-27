/**
 * @file RendererDescriptors.cpp
 * @author Silmaen
 * @date 20/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/gpu/RendererDescriptors.h"

#include "renderer/gpu/RenderCommand.h"
#include "vulkan/internal/RendererDescriptors.h"

namespace owl::renderer::gpu {

namespace {

auto toVkDescriptorType(const BindingType iType) -> VkDescriptorType {
	switch (iType) {
		case BindingType::UniformBuffer:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case BindingType::CombinedImageSampler:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case BindingType::StorageBuffer:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}
	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

auto toVkShaderStages(const uint32_t iStages) -> VkShaderStageFlags {
	VkShaderStageFlags result = 0;
	if ((iStages & ShaderStage::Vertex) != 0U)
		result |= VK_SHADER_STAGE_VERTEX_BIT;
	if ((iStages & ShaderStage::Fragment) != 0U)
		result |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if ((iStages & ShaderStage::Compute) != 0U)
		result |= VK_SHADER_STAGE_COMPUTE_BIT;
	return result;
}

auto getOwnedBlocks() -> std::unordered_map<std::string, uniq<vulkan::internal::RendererDescriptors>>& {
	static std::unordered_map<std::string, uniq<vulkan::internal::RendererDescriptors>> sBlocks;
	return sBlocks;
}
}// namespace

void RendererDescriptors::declare(const std::string& iRenderer, std::span<const BindingDecl> iBindings) {
	// Null and OpenGL backends have no descriptor-set concept — no-op.
	if (const auto api = RenderCommand::getApi(); api != RenderAPI::Type::Vulkan) {
		return;
	}

	std::vector<vulkan::internal::RendererDescriptors::BindingDecl> vkBindings;
	vkBindings.reserve(iBindings.size());
	for (const auto& bd: iBindings) {
		vkBindings.push_back({.binding = bd.binding,
							  .type = toVkDescriptorType(bd.type),
							  .count = bd.count,
							  .stages = toVkShaderStages(bd.stages)});
	}
	auto block = mkUniq<vulkan::internal::RendererDescriptors>(iRenderer);
	block->init(vkBindings);
	getOwnedBlocks()[iRenderer] = std::move(block);
}

void RendererDescriptors::release(const std::string& iRenderer) { getOwnedBlocks().erase(iRenderer); }

void RendererDescriptors::releaseAll() { getOwnedBlocks().clear(); }

RendererDescriptors::ScopedActive::ScopedActive(const std::string& iRenderer) {
	if (const auto api = RenderCommand::getApi(); api != RenderAPI::Type::Vulkan) {
		return;
	}
	auto* const block = vulkan::internal::RendererDescriptors::getForRenderer(iRenderer);
	if (block == nullptr) {
		return;
	}
	mp_state = vulkan::internal::RendererDescriptors::getActive();
	vulkan::internal::RendererDescriptors::setActive(block);
	m_engaged = true;
}

RendererDescriptors::ScopedActive::~ScopedActive() {
	if (!m_engaged)
		return;
	vulkan::internal::RendererDescriptors::setActive(static_cast<vulkan::internal::RendererDescriptors*>(mp_state));
}

}// namespace owl::renderer::gpu
