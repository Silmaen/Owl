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
#include "renderer/gpu/vulkan/StorageBuffer.h"
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
	AllocatedBuffer uniform;// Zeroed uniform buffer.
	AllocatedBuffer storage;// Zeroed storage buffer.
	TextureData image;// 1x1 opaque white texture.
};

auto getDefaults() -> DefaultResources& {
	static DefaultResources sDefaults;
	return sDefaults;
}

auto createZeroedBuffer(const VkBufferUsageFlags iUsage, const std::string_view iName) -> AllocatedBuffer {
	auto buffer = createBuffer(g_defaultBufferSize, iUsage, MemoryUsage::Upload, iName);
	const std::vector<uint8_t> zeros(g_defaultBufferSize, 0);
	writeMapped(buffer, zeros.data(), zeros.size());
	return buffer;
}

void createWhiteImage(TextureData& oImage) {
	constexpr uint32_t white = 0xffffffff;
	auto staging = createBuffer(sizeof(white), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryUsage::Upload, "staging");
	writeMapped(staging, &white, sizeof(white));
	oImage.debugName = "rd.defaultWhite";
	oImage.createImage({1, 1});
	VulkanHandler::get().recordTransfer([&oImage, &staging](VkCommandBuffer iCmd) -> void {
		transitionImageLayout(iCmd, oImage.textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(iCmd, staging.buffer, oImage.textureImage, {1, 1});
		transitionImageLayout(iCmd, oImage.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});
	releaseBuffer(staging);
	oImage.createView();
	oImage.createSampler();
}

void ensureDefaults() {
	auto& defaults = getDefaults();
	if (defaults.uniform.buffer == nullptr)
		defaults.uniform = createZeroedBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "rd.defaultUniform");
	if (defaults.storage.buffer == nullptr)
		defaults.storage = createZeroedBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "rd.defaultStorage");
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
	freeBuffer(defaults.uniform);
	freeBuffer(defaults.storage);
	// TextureData::freeTexture would recreate the global single-image pool, already released at this point.
	auto& image = defaults.image;
	if (image.textureSampler != nullptr)
		vkDestroySampler(device, image.textureSampler, nullptr);
	if (image.textureImageView != nullptr)
		vkDestroyImageView(device, image.textureImageView, nullptr);
	AllocatedImage allocated{.image = image.textureImage, .allocation = image.textureImageMemory};
	MemoryAllocator::get().destroyImage(allocated);
	defaults = DefaultResources{};
}

void RendererDescriptors::registerUniform(const uint32_t iBinding, const uint32_t iSize) {
	auto& ubo = m_uniformBindings[iBinding];
	ubo.shadow.assign(iSize, 0);
	ubo.size = iSize;
	ubo.slice = {};
	ubo.sliceSerial = 0;
	ubo.dirty = true;
	m_dirty = true;
}

void RendererDescriptors::setUniformData(const uint32_t iBinding, const void* iData, const size_t iSize) {
	const auto it = m_uniformBindings.find(iBinding);
	if (it == m_uniformBindings.end()) {
		OWL_CORE_WARN("Vulkan RendererDescriptors[{}]: setUniformData on unregistered binding {}.", m_rendererName,
					  iBinding)
		return;
	}
	if (iData == nullptr || iSize == 0)
		return;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(it->second.shadow.data(), iData, std::min<size_t>(iSize, it->second.shadow.size()));
	OWL_DIAG_POP

	it->second.dirty = true;
	m_dirty = true;
}

void RendererDescriptors::bindStorageBuffer(const uint32_t iBinding, StorageBuffer* iBuffer) {
	auto& binding = m_storageBindings[iBinding];
	if (binding.buffer == iBuffer)
		return;
	binding.buffer = iBuffer;
	binding.version = 0;
	m_dirty = true;
}

void RendererDescriptors::unbindStorageBuffer(const StorageBuffer* iBuffer) {
	if (iBuffer == nullptr)
		return;
	for (auto* const desc: getRegistry() | std::views::values) {
		for (auto& ssbo: desc->m_storageBindings | std::views::values) {
			if (ssbo.buffer == iBuffer) {
				ssbo.buffer = nullptr;
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

auto RendererDescriptors::resolveUniform(UboBinding& ioUbo) -> VkDescriptorBufferInfo {
	auto& vkh = VulkanHandler::get();
	if (ioUbo.dirty || ioUbo.sliceSerial != vkh.getFrameSerial() || ioUbo.slice.buffer == nullptr) {
		ioUbo.slice = vkh.allocateTransient(ioUbo.size);
		if (ioUbo.slice.data == nullptr)
			return {};

		OWL_DIAG_PUSH
		OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
		memcpy(ioUbo.slice.data, ioUbo.shadow.data(), ioUbo.shadow.size());
		OWL_DIAG_POP

		ioUbo.sliceSerial = vkh.getFrameSerial();
		ioUbo.dirty = false;
	}
	return {.buffer = ioUbo.slice.buffer, .offset = ioUbo.slice.offset, .range = ioUbo.size};
}

auto RendererDescriptors::hasStaleStorage() const -> bool {
	return std::ranges::any_of(m_storageBindings | std::views::values, [](const StorageBinding& iBinding) -> bool {
		return iBinding.buffer != nullptr && iBinding.buffer->resolve().version != iBinding.version;
	});
}

void RendererDescriptors::writeDescriptor(VkDescriptorSet iSet) {
	if (iSet == nullptr)
		return;
	const auto& core = VulkanCore::get();
	const auto& defaults = getDefaults();

	m_bufferInfos.clear();
	m_bufferInfos.reserve(m_bindings.size());
	m_imageInfos.clear();
	m_writes.clear();
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
			VkDescriptorBufferInfo info{.buffer = defaults.uniform.buffer, .offset = 0, .range = g_defaultBufferSize};
			if (const auto it = m_uniformBindings.find(binding); it != m_uniformBindings.end()) {
				if (const auto resolved = resolveUniform(it->second); resolved.buffer != nullptr)
					info = resolved;
			}
			if (info.buffer == nullptr)
				continue;
			write.pBufferInfo = &m_bufferInfos.emplace_back(info);
		} else if (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
			VkDescriptorBufferInfo info{.buffer = defaults.storage.buffer, .offset = 0, .range = g_defaultBufferSize};
			if (const auto it = m_storageBindings.find(binding);
				it != m_storageBindings.end() && it->second.buffer != nullptr) {
				if (const auto view = it->second.buffer->resolve(); view.buffer != nullptr) {
					info = {.buffer = view.buffer, .offset = view.offset, .range = view.range};
					it->second.version = view.version;
				}
			}
			if (info.buffer == nullptr)
				continue;
			write.pBufferInfo = &m_bufferInfos.emplace_back(info);
		} else if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && binding == m_textureArrayBinding) {
			if (!collectImageInfos(count))
				continue;
			write.descriptorCount = static_cast<uint32_t>(m_imageInfos.size());
			write.pImageInfo = m_imageInfos.data();
		} else {
			continue;
		}
		m_writes.push_back(write);
	}
	if (!m_writes.empty()) {
		vkUpdateDescriptorSets(core.getLogicalDevice(), static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0,
							   nullptr);
	}
}

auto RendererDescriptors::collectImageInfos(const uint32_t iCount) -> bool {
	const auto& defaults = getDefaults();
	const VkDescriptorImageInfo fallback{.sampler = defaults.image.textureSampler,
										 .imageView = defaults.image.textureImageView,
										 .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	m_imageInfos.clear();
	if (fallback.sampler == nullptr || fallback.imageView == nullptr)
		return false;
	auto& globalTextures = Descriptors::get();
	m_imageInfos.reserve(iCount);
	for (const auto& id: m_textureBind) {
		if (m_imageInfos.size() >= iCount)
			break;
		if (!globalTextures.isTextureRegistered(id)) {
			m_imageInfos.push_back(fallback);
			continue;
		}
		const auto& texData = globalTextures.getTextureData(id);
		if (texData.textureSampler == nullptr || texData.textureImageView == nullptr) {
			m_imageInfos.push_back(fallback);
			continue;
		}
		m_imageInfos.push_back({.sampler = texData.textureSampler,
								.imageView = texData.textureImageView,
								.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
	}
	while (m_imageInfos.size() < iCount) m_imageInfos.push_back(fallback);
	return true;
}

auto RendererDescriptors::getDescriptorSet(const uint32_t iFrame) -> VkDescriptorSet* {
	const uint64_t serial = VulkanHandler::get().getFrameSerial();
	if (m_dirty || m_setSerial != serial || *m_ring.currentPtr() == nullptr || hasStaleStorage()) {
		if (VkDescriptorSet set = m_ring.acquire(iFrame, serial); set != nullptr)
			writeDescriptor(set);
		m_setSerial = serial;
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
