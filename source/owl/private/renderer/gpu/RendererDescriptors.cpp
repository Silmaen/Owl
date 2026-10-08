/**
 * @file RendererDescriptors.cpp
 * @author Silmaen
 * @date 20/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/gpu/RendererDescriptors.h"

#include "opengl/BindingTable.h"
#include "renderer/gpu/RenderCommand.h"
#if OWL_WITH_RENDER
#include "vulkan/internal/RendererDescriptors.h"
#endif

#include <cstdint>
#include <span>

namespace owl::renderer::gpu {

#if OWL_WITH_RENDER
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
	if (const auto api = RenderCommand::getApi(); api == RenderAPI::Type::OpenGL) {
		opengl::BindingTable::release(iRenderer);
		static_cast<void>(opengl::BindingTable::getForRenderer(iRenderer));
		return;
	}
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

void RendererDescriptors::release(const std::string& iRenderer) {
	getOwnedBlocks().erase(iRenderer);
	opengl::BindingTable::release(iRenderer);
}

void RendererDescriptors::releaseAll() {
	getOwnedBlocks().clear();
	opengl::BindingTable::releaseAll();
}

RendererDescriptors::ScopedActive::ScopedActive(const std::string& iRenderer) {
	if (const auto api = RenderCommand::getApi(); api == RenderAPI::Type::OpenGL) {
		mp_state = opengl::BindingTable::getActive();
		opengl::BindingTable::setActive(&opengl::BindingTable::getForRenderer(iRenderer));
		m_engaged = true;
		m_openGl = true;
		return;
	}
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
	if (m_openGl) {
		opengl::BindingTable::setActive(static_cast<opengl::BindingTable*>(mp_state));
		return;
	}
	vulkan::internal::RendererDescriptors::setActive(static_cast<vulkan::internal::RendererDescriptors*>(mp_state));
}
#else
// GPU backends not built (OWL_MODULE_RENDER=OFF): the Null backend has no descriptor set.
void RendererDescriptors::declare([[maybe_unused]] const std::string& iRenderer,
								  [[maybe_unused]] std::span<const BindingDecl> iBindings) {}

void RendererDescriptors::release([[maybe_unused]] const std::string& iRenderer) {}

void RendererDescriptors::releaseAll() {}

RendererDescriptors::ScopedActive::ScopedActive([[maybe_unused]] const std::string& iRenderer) {}

RendererDescriptors::ScopedActive::~ScopedActive() {
	// Never engaged without the Vulkan backend: nothing to restore.
	m_engaged = false;
	m_openGl = false;
	mp_state = nullptr;
}
#endif

}// namespace owl::renderer::gpu
