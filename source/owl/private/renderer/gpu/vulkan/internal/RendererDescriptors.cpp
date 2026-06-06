/**
 * @file RendererDescriptors.cpp
 * @author Silmaen
 * @date 20/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "RendererDescriptors.h"

#include "VulkanCore.h"
#include "VulkanHandler.h"
#include "utils.h"

#include <cstring>

namespace owl::renderer::gpu::vulkan::internal {

namespace {

auto getRegistry() -> std::unordered_map<std::string, RendererDescriptors*>& {
	static std::unordered_map<std::string, RendererDescriptors*> sRegistry;
	return sRegistry;
}
}// namespace

RendererDescriptors::RendererDescriptors(std::string iRendererName) : m_rendererName{std::move(iRendererName)} {
	auto& registry = getRegistry();
	if (registry.contains(m_rendererName)) {
		OWL_CORE_WARN("Vulkan RendererDescriptors: renderer '{}' already registered — overwriting.", m_rendererName)
	}
	registry[m_rendererName] = this;
}

RendererDescriptors::~RendererDescriptors() {
	release();
	auto& registry = getRegistry();
	if (const auto it = registry.find(m_rendererName); it != registry.end() && it->second == this) {
		registry.erase(it);
	}
}

void RendererDescriptors::init(std::span<const BindingDecl> iBindings) {
	release();

	m_bindings.assign(iBindings.begin(), iBindings.end());
	m_textureArrayBinding = std::numeric_limits<uint32_t>::max();
	m_textureArrayCount = 0;
	for (const auto& bd: m_bindings) {
		if (bd.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
			m_textureArrayBinding = bd.binding;
			m_textureArrayCount = bd.count;
			break;// only one texture array binding is supported
		}
	}

	const auto& core = VulkanCore::get();
	auto* const device = core.getLogicalDevice();

	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(m_bindings.size());
	for (const auto& bd: m_bindings) {
		poolSizes.push_back({.type = bd.type, .descriptorCount = bd.count * g_maxFrameInFlight});
	}
	const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
											  .maxSets = g_maxFrameInFlight,
											  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
											  .pPoolSizes = poolSizes.data()};
	if (const auto result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan RendererDescriptors[{}]: failed to create descriptor pool ({}).", m_rendererName,
					   resultString(result))
		return;
	}

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	layoutBindings.reserve(m_bindings.size());
	for (const auto& [binding, type, count, stages]: m_bindings) {
		layoutBindings.push_back({.binding = binding,
								  .descriptorType = type,
								  .descriptorCount = count,
								  .stageFlags = stages,
								  .pImmutableSamplers = nullptr});
	}
	const VkDescriptorSetLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
													 .pNext = nullptr,
													 .flags = {},
													 .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
													 .pBindings = layoutBindings.data()};
	if (const auto result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_layout);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan RendererDescriptors[{}]: failed to create descriptor set layout ({}).", m_rendererName,
					   resultString(result))
		return;
	}

	std::vector layouts(g_maxFrameInFlight, m_layout);
	const VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												.pNext = nullptr,
												.descriptorPool = m_pool,
												.descriptorSetCount = g_maxFrameInFlight,
												.pSetLayouts = layouts.data()};
	m_sets.resize(g_maxFrameInFlight);
	if (const auto result = vkAllocateDescriptorSets(device, &allocInfo, m_sets.data()); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan RendererDescriptors[{}]: failed to allocate descriptor sets ({}).", m_rendererName,
					   resultString(result))
	}
}

void RendererDescriptors::release() {
	if (VulkanHandler::get().getState() != VulkanHandler::State::Running) {
		m_uniformBindings.clear();
		m_storageBindings.clear();
		m_textureBind.clear();
		m_sets.clear();
		m_layout = nullptr;
		m_pool = nullptr;
		return;
	}
	const auto& core = VulkanCore::get();
	auto* const device = core.getLogicalDevice();

	for (auto& ubo: m_uniformBindings | std::views::values) {
		for (size_t i = 0; i < ubo.buffers.size(); ++i) {
			if (i < ubo.mapped.size() && ubo.mapped[i] != nullptr) {
				vkUnmapMemory(device, ubo.memory[i]);
				ubo.mapped[i] = nullptr;
			}
			if (ubo.buffers[i] != nullptr) {
				vkDestroyBuffer(device, ubo.buffers[i], nullptr);
				ubo.buffers[i] = nullptr;
			}
			if (i < ubo.memory.size() && ubo.memory[i] != nullptr) {
				vkFreeMemory(device, ubo.memory[i], nullptr);
				ubo.memory[i] = nullptr;
			}
		}
	}
	m_uniformBindings.clear();
	m_storageBindings.clear();
	m_textureBind.clear();

	if (!m_sets.empty() && m_pool != nullptr) {
		vkFreeDescriptorSets(device, m_pool, static_cast<uint32_t>(m_sets.size()), m_sets.data());
	}
	m_sets.clear();
	if (m_layout != nullptr) {
		vkDestroyDescriptorSetLayout(device, m_layout, nullptr);
		m_layout = nullptr;
	}
	if (m_pool != nullptr) {
		vkDestroyDescriptorPool(device, m_pool, nullptr);
		m_pool = nullptr;
	}
}

void RendererDescriptors::registerUniform(const uint32_t iBinding, const uint32_t iSize) {
	const auto& core = VulkanCore::get();
	auto* const device = core.getLogicalDevice();
	auto& [buffers, memory, mapped, size] = m_uniformBindings[iBinding];
	for (size_t i = 0; i < buffers.size() && i < g_maxFrameInFlight; ++i) {
		if (i < mapped.size() && mapped[i] != nullptr)
			vkUnmapMemory(device, memory[i]);
		if (buffers[i] != nullptr)
			vkDestroyBuffer(device, buffers[i], nullptr);
		if (i < memory.size() && memory[i] != nullptr)
			vkFreeMemory(device, memory[i], nullptr);
	}
	buffers.assign(g_maxFrameInFlight, nullptr);
	memory.assign(g_maxFrameInFlight, nullptr);
	mapped.assign(g_maxFrameInFlight, nullptr);
	size = iSize;
	for (size_t i = 0; i < g_maxFrameInFlight; i++) {
		createBuffer(iSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffers[i], memory[i]);
		if (const auto result = vkMapMemory(device, memory[i], 0, iSize, 0, &mapped[i]); result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan RendererDescriptors[{}]: failed to map UBO (binding={}, frame={}).", m_rendererName,
						   iBinding, i)
		}
	}
}

void RendererDescriptors::setUniformData(const uint32_t iBinding, const void* iData, const size_t iSize) const {
	const auto& vkh = VulkanHandler::get();
	const auto it = m_uniformBindings.find(iBinding);
	if (it == m_uniformBindings.end()) {
		OWL_CORE_WARN("Vulkan RendererDescriptors[{}]: setUniformData on unregistered binding {}.", m_rendererName,
					  iBinding)
		return;
	}
	const auto frame = vkh.getCurrentFrameIndex();
	if (frame >= it->second.mapped.size() || it->second.mapped[frame] == nullptr)
		return;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(it->second.mapped[frame], iData, iSize);
	OWL_DIAG_POP
}

void RendererDescriptors::bindStorageBuffer(const uint32_t iBinding, VkBuffer iBuffer, const VkDeviceSize iSize) {
	auto& [buffer, size] = m_storageBindings[iBinding];
	if (buffer == iBuffer && size == iSize)
		return;
	buffer = iBuffer;
	size = iSize;
	updateDescriptors();
}

void RendererDescriptors::unbindStorageBuffer(VkBuffer iBuffer) {
	if (iBuffer == nullptr)
		return;
	for (auto* const desc: getRegistry() | std::views::values) {
		for (auto& ssbo: desc->m_storageBindings | std::views::values) {
			if (ssbo.buffer == iBuffer) {
				ssbo.buffer = nullptr;
				ssbo.size = 0;
			}
		}
	}
}

void RendererDescriptors::resetTextureBind() { m_textureBind.clear(); }

void RendererDescriptors::textureBind(const uint32_t iIndex) { m_textureBind.emplace_back(iIndex); }

void RendererDescriptors::commitTextureBind(const size_t iCurrentFrame) { updateDescriptor(iCurrentFrame); }

void RendererDescriptors::updateDescriptors() {
	for (size_t i = 0; i < g_maxFrameInFlight; ++i) updateDescriptor(i);
}

void RendererDescriptors::updateDescriptor(const size_t iFrame) {
	if (iFrame >= m_sets.size() || m_sets[iFrame] == nullptr)
		return;
	const auto& core = VulkanCore::get();

	std::vector<std::pair<uint32_t, VkDescriptorBufferInfo>> uboInfos;
	uboInfos.reserve(m_uniformBindings.size());
	for (const auto& [binding, ubo]: m_uniformBindings) {
		if (iFrame >= ubo.buffers.size() || ubo.buffers[iFrame] == nullptr)
			continue;
		uboInfos.emplace_back(binding, VkDescriptorBufferInfo{
											   .buffer = ubo.buffers[iFrame],
											   .offset = 0,
											   .range = ubo.size,
									   });
	}

	std::vector<std::pair<uint32_t, VkDescriptorBufferInfo>> ssboInfos;
	ssboInfos.reserve(m_storageBindings.size());
	for (const auto& [binding, ssbo]: m_storageBindings) {
		if (ssbo.buffer == nullptr)
			continue;
		ssboInfos.emplace_back(binding, VkDescriptorBufferInfo{
												.buffer = ssbo.buffer,
												.offset = 0,
												.range = ssbo.size,
										});
	}

	std::vector<VkDescriptorImageInfo> imageInfos;
	if (m_textureArrayCount > 0) {
		imageInfos.reserve(m_textureArrayCount);
		VkDescriptorImageInfo fallback{};
		auto& globalTextures = Descriptors::get();
		for (const auto& id: m_textureBind) {
			if (!globalTextures.isTextureRegistered(id)) {
				imageInfos.push_back(fallback);
				continue;
			}
			const auto& texData = globalTextures.getTextureData(id);
			if (texData.textureSampler == nullptr || texData.textureImageView == nullptr) {
				imageInfos.push_back(fallback);
				continue;
			}
			const VkDescriptorImageInfo info{.sampler = texData.textureSampler,
											 .imageView = texData.textureImageView,
											 .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			if (fallback.sampler == nullptr)
				fallback = info;
			imageInfos.push_back(info);
		}
		if (fallback.sampler != nullptr) {
			for (auto& info: imageInfos) {
				if (info.sampler == nullptr)
					info = fallback;
			}
			while (imageInfos.size() < m_textureArrayCount) imageInfos.push_back(fallback);
		}
	}

	std::vector<VkWriteDescriptorSet> writes;
	writes.reserve(uboInfos.size() + ssboInfos.size() + (imageInfos.empty() ? 0 : 1));
	for (const auto& [binding, info]: uboInfos) {
		writes.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						  .pNext = nullptr,
						  .dstSet = m_sets[iFrame],
						  .dstBinding = binding,
						  .dstArrayElement = 0,
						  .descriptorCount = 1,
						  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
						  .pImageInfo = nullptr,
						  .pBufferInfo = &info,
						  .pTexelBufferView = nullptr});
	}
	for (const auto& [binding, info]: ssboInfos) {
		writes.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						  .pNext = nullptr,
						  .dstSet = m_sets[iFrame],
						  .dstBinding = binding,
						  .dstArrayElement = 0,
						  .descriptorCount = 1,
						  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
						  .pImageInfo = nullptr,
						  .pBufferInfo = &info,
						  .pTexelBufferView = nullptr});
	}
	if (!imageInfos.empty() && m_textureArrayBinding != std::numeric_limits<uint32_t>::max()) {
		writes.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						  .pNext = nullptr,
						  .dstSet = m_sets[iFrame],
						  .dstBinding = m_textureArrayBinding,
						  .dstArrayElement = 0,
						  .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
						  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						  .pImageInfo = imageInfos.data(),
						  .pBufferInfo = nullptr,
						  .pTexelBufferView = nullptr});
	}
	if (!writes.empty()) {
		vkUpdateDescriptorSets(core.getLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
							   nullptr);
	}
}

auto RendererDescriptors::getDescriptorSet(const uint32_t iFrame) -> VkDescriptorSet* {
	if (iFrame >= m_sets.size())
		return nullptr;
	return &m_sets[iFrame];
}

auto RendererDescriptors::getForRenderer(const std::string& iName) -> RendererDescriptors* {
	const auto& registry = getRegistry();
	const auto it = registry.find(iName);
	if (it == registry.end())
		return nullptr;
	return it->second;
}

namespace {

thread_local RendererDescriptors* tlActive = nullptr;
}// namespace

void RendererDescriptors::setActive(RendererDescriptors* iActive) { tlActive = iActive; }

auto RendererDescriptors::getActive() -> RendererDescriptors* { return tlActive; }

RendererDescriptors::ScopedActive::ScopedActive(RendererDescriptors* iActive) : m_previous{tlActive} {
	tlActive = iActive;
}

RendererDescriptors::ScopedActive::~ScopedActive() { tlActive = m_previous; }

}// namespace owl::renderer::gpu::vulkan::internal
