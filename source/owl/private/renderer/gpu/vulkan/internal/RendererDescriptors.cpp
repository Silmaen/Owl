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

#include <bit>
#include <cstring>

namespace owl::renderer::gpu::vulkan::internal {

namespace {

auto getRegistry() -> std::unordered_map<std::string, RendererDescriptors*>& {
	static std::unordered_map<std::string, RendererDescriptors*> sRegistry;
	return sRegistry;
}

// Size of the default uniform and storage buffers (covers every uniform block of the engine shaders).
constexpr VkDeviceSize g_defaultBufferSize = 4096;

// Resources written into declared bindings nothing was bound to, so every descriptor a shader reads is valid.
struct DefaultResources {
	VkBuffer uniform = nullptr;// Zeroed uniform buffer.
	VkDeviceMemory uniformMemory = nullptr;// Memory of the uniform buffer.
	VkBuffer storage = nullptr;// Zeroed storage buffer.
	VkDeviceMemory storageMemory = nullptr;// Memory of the storage buffer.
	TextureData image;// 1x1 opaque white texture.
};

auto getDefaults() -> DefaultResources& {
	static DefaultResources sDefaults;
	return sDefaults;
}

void createZeroedBuffer(const VkBufferUsageFlags iUsage, VkBuffer& oBuffer, VkDeviceMemory& oMemory) {
	const auto& core = VulkanCore::get();
	createBuffer(g_defaultBufferSize, iUsage,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, oBuffer, oMemory);
	void* mapped = nullptr;
	if (oMemory == nullptr ||
		vkMapMemory(core.getLogicalDevice(), oMemory, 0, g_defaultBufferSize, 0, &mapped) != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan RendererDescriptors: Failed to map a default buffer.")
		return;
	}

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memset(mapped, 0, g_defaultBufferSize);
	OWL_DIAG_POP

	vkUnmapMemory(core.getLogicalDevice(), oMemory);
}

void createWhiteImage(TextureData& oImage) {
	const auto& core = VulkanCore::get();
	constexpr uint32_t white = 0xffffffff;
	VkBuffer staging = nullptr;
	VkDeviceMemory stagingMemory = nullptr;
	createBuffer(sizeof(white), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMemory);
	void* mapped = nullptr;
	if (vkMapMemory(core.getLogicalDevice(), stagingMemory, 0, sizeof(white), 0, &mapped) == VK_SUCCESS) {

		OWL_DIAG_PUSH
		OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
		memcpy(mapped, &white, sizeof(white));
		OWL_DIAG_POP

		vkUnmapMemory(core.getLogicalDevice(), stagingMemory);
	}
	oImage.debugName = "rd.defaultWhite";
	oImage.createImage({1, 1});
	vkBindImageMemory(core.getLogicalDevice(), oImage.textureImage, oImage.textureImageMemory, 0);
	transitionImageLayout(oImage.textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(staging, oImage.textureImage, {1, 1});
	transitionImageLayout(oImage.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	freeBuffer(core.getLogicalDevice(), staging, stagingMemory);
	oImage.createView();
	oImage.createSampler();
}

void ensureDefaults() {
	auto& defaults = getDefaults();
	if (defaults.uniform == nullptr)
		createZeroedBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, defaults.uniform, defaults.uniformMemory);
	if (defaults.storage == nullptr)
		createZeroedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, defaults.storage, defaults.storageMemory);
	if (defaults.image.textureImage == nullptr)
		createWhiteImage(defaults.image);
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

	ensureDefaults();
	const auto& core = VulkanCore::get();
	auto* const device = core.getLogicalDevice();

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
	core.setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, std::bit_cast<uint64_t>(m_layout),
					   "rd.layout:" + m_rendererName);

	std::vector<VkDescriptorPoolSize> perSetSizes;
	perSetSizes.reserve(m_bindings.size());
	for (const auto& bd: m_bindings) perSetSizes.push_back({.type = bd.type, .descriptorCount = bd.count});
	m_ring.init(m_layout, std::move(perSetSizes));
}

void RendererDescriptors::release() {
	if (VulkanHandler::get().getState() != VulkanHandler::State::Running) {
		m_uniformBindings.clear();
		m_storageBindings.clear();
		m_textureBind.clear();
		m_ring.reset();
		m_layout = nullptr;
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

	m_ring.release();
	if (m_layout != nullptr) {
		vkDestroyDescriptorSetLayout(device, m_layout, nullptr);
		m_layout = nullptr;
	}
}

void RendererDescriptors::releaseDefaults() {
	auto& defaults = getDefaults();
	if (VulkanHandler::get().getState() != VulkanHandler::State::Running) {
		defaults = DefaultResources{};
		return;
	}
	auto* const device = VulkanCore::get().getLogicalDevice();
	if (defaults.uniform != nullptr)
		freeBuffer(device, defaults.uniform, defaults.uniformMemory);
	if (defaults.storage != nullptr)
		freeBuffer(device, defaults.storage, defaults.storageMemory);
	// TextureData::freeTexture would recreate the global single-image pool, already released at this point.
	auto& image = defaults.image;
	if (image.textureSampler != nullptr)
		vkDestroySampler(device, image.textureSampler, nullptr);
	if (image.textureImageView != nullptr)
		vkDestroyImageView(device, image.textureImageView, nullptr);
	if (image.textureImage != nullptr)
		vkDestroyImage(device, image.textureImage, nullptr);
	if (image.textureImageMemory != nullptr)
		vkFreeMemory(device, image.textureImageMemory, nullptr);
	defaults = DefaultResources{};
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
	m_dirty = true;
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
	m_dirty = true;
}

void RendererDescriptors::unbindStorageBuffer(VkBuffer iBuffer) {
	if (iBuffer == nullptr)
		return;
	for (auto* const desc: getRegistry() | std::views::values) {
		for (auto& ssbo: desc->m_storageBindings | std::views::values) {
			if (ssbo.buffer == iBuffer) {
				ssbo.buffer = nullptr;
				ssbo.size = 0;
				desc->m_dirty = true;
			}
		}
	}
}

void RendererDescriptors::resetTextureBind() {
	m_textureBind.clear();
	m_dirty = true;
}

void RendererDescriptors::textureBind(const uint32_t iIndex) {
	m_textureBind.emplace_back(iIndex);
	m_dirty = true;
}

void RendererDescriptors::commitTextureBind() { m_dirty = true; }

void RendererDescriptors::notifySubmitAll(VkFence iFence) {
	for (auto* const desc: getRegistry() | std::views::values) {
		desc->m_ring.onSubmit(iFence);
		desc->m_dirty = true;
	}
}

void RendererDescriptors::writeDescriptor(VkDescriptorSet iSet, const size_t iFrame) {
	if (iSet == nullptr)
		return;
	const auto& core = VulkanCore::get();
	const auto& defaults = getDefaults();

	std::vector<VkDescriptorBufferInfo> bufferInfos;
	bufferInfos.reserve(m_bindings.size());
	std::vector<VkDescriptorImageInfo> imageInfos;
	std::vector<VkWriteDescriptorSet> writes;
	writes.reserve(m_bindings.size());
	for (const auto& [binding, type, count, stages]: m_bindings) {
		VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
								   .pNext = nullptr,
								   .dstSet = iSet,
								   .dstBinding = binding,
								   .dstArrayElement = 0,
								   .descriptorCount = 1,
								   .descriptorType = type,
								   .pImageInfo = nullptr,
								   .pBufferInfo = nullptr,
								   .pTexelBufferView = nullptr};
		if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
			VkDescriptorBufferInfo info{.buffer = defaults.uniform, .offset = 0, .range = g_defaultBufferSize};
			if (const auto it = m_uniformBindings.find(binding); it != m_uniformBindings.end() &&
																 iFrame < it->second.buffers.size() &&
																 it->second.buffers[iFrame] != nullptr)
				info = {.buffer = it->second.buffers[iFrame], .offset = 0, .range = it->second.size};
			if (info.buffer == nullptr)
				continue;
			write.pBufferInfo = &bufferInfos.emplace_back(info);
		} else if (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
			VkDescriptorBufferInfo info{.buffer = defaults.storage, .offset = 0, .range = g_defaultBufferSize};
			if (const auto it = m_storageBindings.find(binding);
				it != m_storageBindings.end() && it->second.buffer != nullptr)
				info = {.buffer = it->second.buffer, .offset = 0, .range = it->second.size};
			if (info.buffer == nullptr)
				continue;
			write.pBufferInfo = &bufferInfos.emplace_back(info);
		} else if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && binding == m_textureArrayBinding) {
			imageInfos = collectImageInfos(count);
			if (imageInfos.empty())
				continue;
			write.descriptorCount = static_cast<uint32_t>(imageInfos.size());
			write.pImageInfo = imageInfos.data();
		} else {
			continue;
		}
		writes.push_back(write);
	}
	if (!writes.empty()) {
		vkUpdateDescriptorSets(core.getLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
							   nullptr);
	}
}

auto RendererDescriptors::collectImageInfos(const uint32_t iCount) const -> std::vector<VkDescriptorImageInfo> {
	const auto& defaults = getDefaults();
	const VkDescriptorImageInfo fallback{.sampler = defaults.image.textureSampler,
										 .imageView = defaults.image.textureImageView,
										 .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	if (fallback.sampler == nullptr || fallback.imageView == nullptr)
		return {};
	auto& globalTextures = Descriptors::get();
	std::vector<VkDescriptorImageInfo> infos;
	infos.reserve(iCount);
	for (const auto& id: m_textureBind) {
		if (infos.size() >= iCount)
			break;
		if (!globalTextures.isTextureRegistered(id)) {
			infos.push_back(fallback);
			continue;
		}
		const auto& texData = globalTextures.getTextureData(id);
		if (texData.textureSampler == nullptr || texData.textureImageView == nullptr) {
			infos.push_back(fallback);
			continue;
		}
		infos.push_back({.sampler = texData.textureSampler,
						 .imageView = texData.textureImageView,
						 .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
	}
	while (infos.size() < iCount) infos.push_back(fallback);
	return infos;
}

auto RendererDescriptors::getDescriptorSet(const uint32_t iFrame) -> VkDescriptorSet* {
	if (m_dirty || *m_ring.currentPtr() == nullptr) {
		if (VkDescriptorSet set = m_ring.acquire(); set != nullptr)
			writeDescriptor(set, iFrame);
		m_dirty = false;
	}
	return m_ring.currentPtr();
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
