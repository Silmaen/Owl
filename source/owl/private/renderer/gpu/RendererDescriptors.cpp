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

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace owl::renderer::gpu {

namespace {

// A renderer block known by its identifier, with the backend blocks cached so `ScopedActive` needs no lookup.
struct BlockSlot {
	// Renderer namespace key.
	std::string name;
#if OWL_WITH_RENDER
	// OpenGL binding table of the block, or nullptr until first used.
	opengl::BindingTable* openGl = nullptr;
	// Vulkan block, or nullptr when not declared.
	vulkan::internal::RendererDescriptors* vulkan = nullptr;
#endif
};

auto getSlots() -> std::vector<BlockSlot>& {
	static std::vector<BlockSlot> sSlots;
	return sSlots;
}

}// namespace

auto RendererDescriptors::getId(const std::string& iRenderer) -> RendererId {
	auto& slots = getSlots();
	for (size_t i = 0; i < slots.size(); ++i)
		if (slots[i].name == iRenderer)
			return static_cast<RendererId>(i);
	slots.push_back(BlockSlot{.name = iRenderer});
	return static_cast<RendererId>(slots.size() - 1);
}

#if OWL_WITH_RENDER
namespace {

auto toVkDescriptorType(const BindingType iType) -> VkDescriptorType {
	switch (iType) {
		case BindingType::UniformBuffer:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case BindingType::CombinedImageSampler:
		case BindingType::CombinedImageSamplerCube:
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
		getSlots()[getId(iRenderer)].openGl = &opengl::BindingTable::getForRenderer(iRenderer);
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
							  .stages = toVkShaderStages(bd.stages),
							  .cube = bd.type == BindingType::CombinedImageSamplerCube});
	}
	auto block = mkUniq<vulkan::internal::RendererDescriptors>(iRenderer);
	block->init(vkBindings);
	getSlots()[getId(iRenderer)].vulkan = block.get();
	getOwnedBlocks()[iRenderer] = std::move(block);
}

void RendererDescriptors::release(const std::string& iRenderer) {
	auto& slot = getSlots()[getId(iRenderer)];
	slot.openGl = nullptr;
	slot.vulkan = nullptr;
	getOwnedBlocks().erase(iRenderer);
	opengl::BindingTable::release(iRenderer);
}

void RendererDescriptors::releaseAll() {
	for (auto& slot: getSlots()) {
		slot.openGl = nullptr;
		slot.vulkan = nullptr;
	}
	getOwnedBlocks().clear();
	opengl::BindingTable::releaseAll();
}

RendererDescriptors::ScopedActive::ScopedActive(const RendererId iRenderer) {
	auto& slots = getSlots();
	if (iRenderer >= slots.size())
		return;
	auto& slot = slots[iRenderer];
	if (const auto api = RenderCommand::getApi(); api == RenderAPI::Type::OpenGL) {
		if (slot.openGl == nullptr)
			slot.openGl = &opengl::BindingTable::getForRenderer(slot.name);
		mp_state = opengl::BindingTable::getActive();
		opengl::BindingTable::setActive(slot.openGl);
		m_engaged = true;
		m_openGl = true;
		return;
	}
	if (const auto api = RenderCommand::getApi(); api != RenderAPI::Type::Vulkan || slot.vulkan == nullptr) {
		return;
	}
	mp_state = vulkan::internal::RendererDescriptors::getActive();
	vulkan::internal::RendererDescriptors::setActive(slot.vulkan);
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

RendererDescriptors::ScopedActive::ScopedActive([[maybe_unused]] const RendererId iRenderer) {}

RendererDescriptors::ScopedActive::~ScopedActive() {
	// Never engaged without the Vulkan backend: nothing to restore.
	m_engaged = false;
	m_openGl = false;
	mp_state = nullptr;
}
#endif

}// namespace owl::renderer::gpu
