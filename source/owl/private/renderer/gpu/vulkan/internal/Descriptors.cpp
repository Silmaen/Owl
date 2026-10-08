/**
 * @file Descriptors.cpp
 * @author Silmaen
 * @date 13/03/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Descriptors.h"

#include "FrameProfiler.h"
#include "VulkanCore.h"
#include "VulkanHandler.h"
#include "utils.h"

#include <bit>
#include <cstring>

namespace owl::renderer::gpu::vulkan::internal {

void TextureData::freeTexture() {
	auto* const device = VulkanCore::get().getLogicalDevice();
	auto* const pool = Descriptors::get().getSingleImageDescriptorPool();
	VulkanHandler::get().deferRelease(
			[device, pool, set = textureDescriptorSet, setLayout = textureDescriptorSetLayout, sampler = textureSampler,
			 view = textureImageView,
			 image = AllocatedImage{.image = textureImage, .allocation = textureImageMemory}]() mutable -> void {
				if (device == nullptr)
					return;
				if (set != nullptr)
					vkFreeDescriptorSets(device, pool, 1, &set);
				if (setLayout != nullptr)
					vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
				if (sampler != nullptr)
					vkDestroySampler(device, sampler, nullptr);
				if (view != nullptr)
					vkDestroyImageView(device, view, nullptr);
				MemoryAllocator::get().destroyImage(image);
			});
	textureDescriptorSet = nullptr;
	textureDescriptorSetLayout = nullptr;
	textureSampler = nullptr;
	textureImageView = nullptr;
	textureImage = nullptr;
	textureImageMemory = nullptr;
}

void TextureData::createDescriptorSet() {
	const auto& pool = Descriptors::get().getSingleImageDescriptorPool();
	const auto& core = VulkanCore::get();
	// ImGui >= 1.92.9 binds its own sampler: the texture id is a sampled-image set.
	static constexpr VkDescriptorSetLayoutBinding imageLayoutBinding{.binding = 0,
																	 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
																	 .descriptorCount = 1,
																	 .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
																	 .pImmutableSamplers = nullptr};
	constexpr VkDescriptorSetLayoutCreateInfo layoutCi{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
													   .pNext = nullptr,
													   .flags = {},
													   .bindingCount = 1,
													   .pBindings = &imageLayoutBinding};
	if (const auto result =
				vkCreateDescriptorSetLayout(core.getLogicalDevice(), &layoutCi, nullptr, &textureDescriptorSetLayout);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture Descriptor: failed to create descriptor set layout ({}).", resultString(result))
	}
	core.setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, std::bit_cast<uint64_t>(textureDescriptorSetLayout),
					   "tex.layout:" + debugName);
	const VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												.pNext = nullptr,
												.descriptorPool = pool,
												.descriptorSetCount = 1,
												.pSetLayouts = &textureDescriptorSetLayout};
	if (const auto result = vkAllocateDescriptorSets(core.getLogicalDevice(), &allocInfo, &textureDescriptorSet);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture Descriptor: failed to allocate descriptor sets ({}).", resultString(result))
	}
	const VkDescriptorImageInfo info{.sampler = textureSampler,
									 .imageView = textureImageView,
									 .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	const VkWriteDescriptorSet wrt{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
								   .pNext = nullptr,
								   .dstSet = textureDescriptorSet,
								   .dstBinding = 0,
								   .dstArrayElement = 0,
								   .descriptorCount = 1,
								   .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
								   .pImageInfo = &info,
								   .pBufferInfo = nullptr,
								   .pTexelBufferView = nullptr};
	vkUpdateDescriptorSets(core.getLogicalDevice(), 1, &wrt, 0, nullptr);
}

void TextureData::createView() {
	const auto& vkc = VulkanCore::get();
	const VkImageViewCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
										   .pNext = nullptr,
										   .flags = {},
										   .image = textureImage,
										   .viewType = VK_IMAGE_VIEW_TYPE_2D,
										   .format = VK_FORMAT_R8G8B8A8_UNORM,
										   .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
														  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
										   .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
																.baseMipLevel = 0,
																.levelCount = mipLevels,
																.baseArrayLayer = 0,
																.layerCount = 1}};
	if (textureImageView != nullptr)
		VulkanHandler::get().deferRelease([device = vkc.getLogicalDevice(), view = textureImageView]() -> void {
			vkDestroyImageView(device, view, nullptr);
		});
	if (const VkResult result = vkCreateImageView(vkc.getLogicalDevice(), &createInfo, nullptr, &textureImageView);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture: Error creating image views ({}).", internal::resultString(result))
	}
	vkc.setObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, std::bit_cast<uint64_t>(textureImageView), "tex.view:" + debugName);
}

void TextureData::createSampler() {
	const auto& vkc = VulkanCore::get();
	const VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
										  .pNext = nullptr,
										  .flags = {},
										  .magFilter = nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
										  .minFilter = nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
										  .mipmapMode = nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST
																: VK_SAMPLER_MIPMAP_MODE_LINEAR,
										  .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .mipLodBias = {},
										  .anisotropyEnable = nearest ? VK_FALSE : VK_TRUE,
										  .maxAnisotropy = nearest ? 1.f : vkc.getMaxSamplerAnisotropy(),
										  .compareEnable = VK_FALSE,
										  .compareOp = VK_COMPARE_OP_ALWAYS,
										  .minLod = 0.f,
										  .maxLod = static_cast<float>(mipLevels),
										  .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
										  .unnormalizedCoordinates = VK_FALSE};
	if (textureSampler != nullptr)
		VulkanHandler::get().deferRelease([device = vkc.getLogicalDevice(), sampler = textureSampler]() -> void {
			vkDestroySampler(device, sampler, nullptr);
		});
	if (const VkResult result = vkCreateSampler(vkc.getLogicalDevice(), &samplerInfo, nullptr, &textureSampler);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture: Error creating texture sampler ({}).", internal::resultString(result))
	}
	vkc.setObjectName(VK_OBJECT_TYPE_SAMPLER, std::bit_cast<uint64_t>(textureSampler), "tex.sampler:" + debugName);
}

void TextureData::createImage(const math::vec2ui& iDimensions) {
	freeTexture();
	const VkImageCreateInfo imageInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.extent = {.width = iDimensions.x(), .height = iDimensions.y(), .depth = 1},
			.mipLevels = mipLevels,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT) |
					 static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
	const auto image = MemoryAllocator::get().createImage(imageInfo, "tex.image:" + debugName);
	textureImage = image.image;
	textureImageMemory = image.allocation;
}

Descriptors::Descriptors() = default;

Descriptors::~Descriptors() { release(); }

void Descriptors::release() {
	const auto& core = VulkanCore::get();
	resetTextureBind();
	m_textureBind.shrink_to_fit();
	if (!m_textures.empty()) {
		for (auto& [id, tex]: m_textures) {
			OWL_CORE_TRACE("Vulkan Descriptors: releasing texture id {}.", id)
			tex->freeTexture();
			tex.reset();
		}
	}
	m_textures.clear();
	for (auto& [binding, ubo]: m_uniformBindings) {
		for (auto& buffer: ubo.buffers) freeBuffer(buffer);
	}
	m_uniformBindings.clear();
	if (m_singleImageDescriptorPool != nullptr) {
		vkDestroyDescriptorPool(core.getLogicalDevice(), m_singleImageDescriptorPool, nullptr);
		m_singleImageDescriptorPool = nullptr;
	}
	if (m_imguiDescriptorPool != nullptr) {
		vkDestroyDescriptorPool(core.getLogicalDevice(), m_imguiDescriptorPool, nullptr);
		m_imguiDescriptorPool = nullptr;
	}
	if (!m_descriptorSets.empty()) {
		vkFreeDescriptorSets(core.getLogicalDevice(), m_descriptorPool, static_cast<uint32_t>(m_descriptorSets.size()),
							 m_descriptorSets.data());
		m_descriptorSets.clear();
		m_descriptorSets.shrink_to_fit();
	}
	if (m_descriptorSetLayout != nullptr) {
		vkDestroyDescriptorSetLayout(core.getLogicalDevice(), m_descriptorSetLayout, nullptr);
		m_descriptorSetLayout = nullptr;
	}
	if (m_descriptorPool != nullptr) {
		vkDestroyDescriptorPool(core.getLogicalDevice(), m_descriptorPool, nullptr);
		m_descriptorPool = nullptr;
	}
}

void Descriptors::createDescriptors() {
	const auto& core = VulkanCore::get();
	// Descriptor pools.
	std::vector<VkDescriptorPoolSize> poolSizes{
			{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 32 * g_maxFrameInFlight},
			{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 32 * g_maxFrameInFlight},
	};
	const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
											  .maxSets = g_maxFrameInFlight,
											  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
											  .pPoolSizes = poolSizes.data()};
	if (const VkResult result = vkCreateDescriptorPool(core.getLogicalDevice(), &poolInfo, nullptr, &m_descriptorPool);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Descriptors: failed to create descriptor pool ({}).", resultString(result))
	}

	constexpr VkDescriptorSetLayoutBinding uboLayoutBinding{.binding = 0,
															.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
															.descriptorCount = 1,
															.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
															.pImmutableSamplers = nullptr};
	constexpr VkDescriptorSetLayoutBinding imageLayoutBinding{.binding = 1,
															  .descriptorType =
																	  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
															  .descriptorCount = 32,
															  .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
															  .pImmutableSamplers = nullptr};
	constexpr VkDescriptorSetLayoutBinding drawUboLayoutBinding{.binding = 2,
																.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
																.descriptorCount = 1,
																.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
																			  VK_SHADER_STAGE_FRAGMENT_BIT,
																.pImmutableSamplers = nullptr};
	std::vector bindings = {uboLayoutBinding, imageLayoutBinding, drawUboLayoutBinding};
	const VkDescriptorSetLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
													 .pNext = nullptr,
													 .flags = {},
													 .bindingCount = static_cast<uint32_t>(bindings.size()),
													 .pBindings = bindings.data()};
	if (const auto result =
				vkCreateDescriptorSetLayout(core.getLogicalDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Descriptor: failed to create descriptor set layout ({}).", resultString(result))
	}

	// Descriptor sets
	std::vector layouts(g_maxFrameInFlight, m_descriptorSetLayout);
	const VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												.pNext = nullptr,
												.descriptorPool = m_descriptorPool,
												.descriptorSetCount = g_maxFrameInFlight,
												.pSetLayouts = layouts.data()};
	m_descriptorSets.resize(g_maxFrameInFlight);
	if (const auto result = vkAllocateDescriptorSets(core.getLogicalDevice(), &allocInfo, m_descriptorSets.data());
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Descriptor: failed to allocate descriptor sets ({}).", resultString(result))
	}
}

void Descriptors::updateDescriptors() {
	for (size_t i = 0; i < g_maxFrameInFlight; i++) { updateDescriptor(i); }
}

void Descriptors::updateDescriptor(const size_t iFrame) {
	const auto& core = VulkanCore::get();
	std::vector<std::pair<uint32_t, VkDescriptorBufferInfo>> uboInfos;
	uboInfos.reserve(m_uniformBindings.size());
	for (const auto& [binding, ubo]: m_uniformBindings) {
		if (iFrame >= ubo.buffers.size() || ubo.buffers[iFrame].buffer == nullptr)
			continue;
		uboInfos.emplace_back(binding, VkDescriptorBufferInfo{
											   .buffer = ubo.buffers[iFrame].buffer,
											   .offset = 0,
											   .range = ubo.size,
									   });
	}
	constexpr uint32_t maxSamplerDescriptors = 32;
	std::vector<VkDescriptorImageInfo> imageInfos;
	imageInfos.reserve(maxSamplerDescriptors);
	VkDescriptorImageInfo fallbackImageInfo{};
	for (const auto& id: m_textureBind) {
		const auto texData = m_textures.getTextureData(id);
		if (texData == nullptr || texData->textureSampler == nullptr || texData->textureImageView == nullptr) {
			imageInfos.push_back(fallbackImageInfo);
			continue;
		}
		const VkDescriptorImageInfo info{.sampler = texData->textureSampler,
										 .imageView = texData->textureImageView,
										 .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		if (fallbackImageInfo.sampler == nullptr)
			fallbackImageInfo = info;
		imageInfos.push_back(info);
	}
	// Fill remaining slots with the first valid texture to keep all descriptors valid.
	if (fallbackImageInfo.sampler != nullptr) {
		for (auto& info: imageInfos) {
			if (info.sampler == nullptr)
				info = fallbackImageInfo;
		}
		while (imageInfos.size() < maxSamplerDescriptors) { imageInfos.push_back(fallbackImageInfo); }
	}
	std::vector<VkWriteDescriptorSet> descriptorWrites;
	descriptorWrites.reserve(uboInfos.size() + 1);// one per UBO binding + one for the texture array
	for (const auto& [binding, info]: uboInfos) {
		descriptorWrites.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
									.pNext = nullptr,
									.dstSet = m_descriptorSets[iFrame],
									.dstBinding = binding,
									.dstArrayElement = 0,
									.descriptorCount = 1,
									.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
									.pImageInfo = nullptr,
									.pBufferInfo = &info,
									.pTexelBufferView = nullptr});
	}
	if (!imageInfos.empty()) {
		descriptorWrites.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
									.pNext = nullptr,
									.dstSet = m_descriptorSets[iFrame],
									.dstBinding = 1,
									.dstArrayElement = 0,
									.descriptorCount = static_cast<uint32_t>(imageInfos.size()),
									.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
									.pImageInfo = imageInfos.data(),
									.pBufferInfo = nullptr,
									.pTexelBufferView = nullptr});
	}
	if (!descriptorWrites.empty()) {
		vkUpdateDescriptorSets(core.getLogicalDevice(), static_cast<uint32_t>(descriptorWrites.size()),
							   descriptorWrites.data(), 0, nullptr);
	}
}

void Descriptors::registerUniform(const uint32_t iBinding, const uint32_t iSize) {
	auto& ubo = m_uniformBindings[iBinding];
	for (auto& buffer: ubo.buffers) freeBuffer(buffer);
	ubo.buffers.assign(g_maxFrameInFlight, {});
	ubo.size = iSize;
	for (auto& buffer: ubo.buffers)
		buffer = createBuffer(iSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryUsage::Upload, "descriptors.ubo");
}

void Descriptors::setUniformData(const uint32_t iBinding, const void* iData, const size_t iSize) const {
	const auto& vkh = VulkanHandler::get();
	const auto it = m_uniformBindings.find(iBinding);
	if (it == m_uniformBindings.end()) {
		OWL_CORE_WARN("Vulkan Descriptors: setUniformData on unregistered binding {}.", iBinding)
		return;
	}
	const auto frame = vkh.getCurrentFrameIndex();
	if (frame >= it->second.buffers.size())
		return;
	writeMapped(it->second.buffers[frame], iData, iSize);
}

auto Descriptors::registerNewTexture() -> uint32_t { return m_textures.registerNewTexture(); }

auto Descriptors::isTextureRegistered(const uint32_t iIndex) const -> bool {
	if (iIndex == 0)
		return false;
	return m_textures.contains(iIndex);
}

auto Descriptors::getTextureData(const uint32_t iIndex) -> TextureData& { return *m_textures.getTextureData(iIndex); }

void Descriptors::unregisterTexture(const uint32_t iIndex) { m_textures.unregisterTexture(iIndex); }

void Descriptors::resetTextureBind() { m_textureBind.clear(); }

void Descriptors::commitTextureBind(const size_t iCurrentFrame) { updateDescriptor(iCurrentFrame); }

void Descriptors::textureBind(const uint32_t iIndex) { m_textureBind.emplace_back(iIndex); }

void Descriptors::createImguiDescriptorPool() {
	if (m_imguiDescriptorPool != nullptr)
		return;
	const auto& core = VulkanCore::get();
	// ImGui >= 1.92.9 allocates its two sampler sets and every texture set from this pool.
	std::vector<VkDescriptorPoolSize> poolSizes{
			{.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = 1000},
			{.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, .descriptorCount = 1000},
	};
	const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
											  .maxSets = 1000,
											  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
											  .pPoolSizes = poolSizes.data()};
	if (const VkResult result =
				vkCreateDescriptorPool(core.getLogicalDevice(), &poolInfo, nullptr, &m_imguiDescriptorPool);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Descriptors: failed to create descriptor pool ({}).", resultString(result))
	}
}

void Descriptors::createSingleImageDescriptorPool() {
	if (m_singleImageDescriptorPool != nullptr)
		return;
	const auto& core = VulkanCore::get();
	// Descriptor pools.
	std::vector<VkDescriptorPoolSize> poolSizes{
			{.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1000},
	};
	const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
											  .maxSets = 1024,
											  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
											  .pPoolSizes = poolSizes.data()};
	if (const VkResult result =
				vkCreateDescriptorPool(core.getLogicalDevice(), &poolInfo, nullptr, &m_singleImageDescriptorPool);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Descriptors: failed to create descriptor pool ({}).", resultString(result))
	}
}

auto Descriptors::TextureList::contains(uint32_t iIndex) const -> bool {
	return std::find_if(textures.begin(), textures.end(),
						[&iIndex](const auto& iElem) -> bool { return iElem.first == iIndex; }) != textures.end();
}

auto Descriptors::TextureList::registerNewTexture() -> uint32_t {
	++nextId;
	textures.emplace_back(nextId, mkShared<TextureData>());
	return nextId;
}

void Descriptors::TextureList::unregisterTexture(uint32_t iIndex) {
	const auto iter = std::find_if(textures.begin(), textures.end(),
								   [&iIndex](const auto& iElem) -> bool { return iElem.first == iIndex; });
	if (iter == textures.end())
		return;
	iter->second->freeTexture();
	textures.erase(iter);
}

auto Descriptors::TextureList::getTextureData(uint32_t iIndex) -> tex {
	const auto iter = std::find_if(textures.begin(), textures.end(),
								   [&iIndex](const auto& iElem) -> bool { return iElem.first == iIndex; });
	if (iter == textures.end())
		return nullptr;
	return iter->second;
}

auto Descriptors::get() -> Descriptors& {
	static Descriptors instance;
	return instance;
}

}// namespace owl::renderer::gpu::vulkan::internal
