/**
 * @file Descriptors.cpp
 * @author Silmaen
 * @date 13/03/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Descriptors.h"

#include "VulkanCore.h"
#include "VulkanHandler.h"
#include "utils.h"

namespace owl::renderer::vulkan::internal {


void TextureData::freeTexture() {
	const auto& core = internal::VulkanCore::get();
	const auto& pool = Descriptors::get().getSingleImageDescriptorPool();
	vkDeviceWaitIdle(core.getLogicalDevice());
	if (textureDescriptorSet != nullptr) {
		vkFreeDescriptorSets(core.getLogicalDevice(), pool, 1, &textureDescriptorSet);
		textureDescriptorSet = nullptr;
	}
	if (textureDescriptorSetLayout != nullptr) {
		vkDestroyDescriptorSetLayout(core.getLogicalDevice(), textureDescriptorSetLayout, nullptr);
		textureDescriptorSetLayout = nullptr;
	}
	if (textureSampler != nullptr) {
		vkDestroySampler(core.getLogicalDevice(), textureSampler, nullptr);
		textureSampler = nullptr;
	}
	if (textureImageView != nullptr) {
		vkDestroyImageView(core.getLogicalDevice(), textureImageView, nullptr);
		textureImageView = nullptr;
	}
	if (textureImage != nullptr) {
		vkDestroyImage(core.getLogicalDevice(), textureImage, nullptr);
		textureImage = nullptr;
	}
	if (textureImageMemory != nullptr) {
		vkFreeMemory(core.getLogicalDevice(), textureImageMemory, nullptr);
		textureImageMemory = nullptr;
	}
}

void TextureData::createDescriptorSet() {
	const auto& pool = Descriptors::get().getSingleImageDescriptorPool();
	const auto& core = VulkanCore::get();
	static constexpr VkDescriptorSetLayoutBinding samplerLayoutBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr};
	constexpr VkDescriptorSetLayoutCreateInfo layoutCi{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
													   .pNext = nullptr,
													   .flags = {},
													   .bindingCount = 1,
													   .pBindings = &samplerLayoutBinding};
	if (const auto result =
				vkCreateDescriptorSetLayout(core.getLogicalDevice(), &layoutCi, nullptr, &textureDescriptorSetLayout);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture Descriptor: failed to create descriptor set layout ({}).", resultString(result))
	}
	const VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												.pNext = nullptr,
												.descriptorPool = pool,
												.descriptorSetCount = 1,
												.pSetLayouts = &textureDescriptorSetLayout};
	if (const auto result = vkAllocateDescriptorSets(core.getLogicalDevice(), &allocInfo, &textureDescriptorSet);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture Descriptor: failed to allocate descriptor sets ({})", resultString(result))
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
								   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
								   .pImageInfo = &info,
								   .pBufferInfo = nullptr,
								   .pTexelBufferView = nullptr};
	vkUpdateDescriptorSets(core.getLogicalDevice(), 1, &wrt, 0, nullptr);
}

void TextureData::createView() {
	const auto& vkc = internal::VulkanCore::get();
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
																.levelCount = 1,
																.baseArrayLayer = 0,
																.layerCount = 1}};
	if (textureImageView != nullptr)
		vkDestroyImageView(vkc.getLogicalDevice(), textureImageView, nullptr);
	if (const VkResult result = vkCreateImageView(vkc.getLogicalDevice(), &createInfo, nullptr, &textureImageView);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture: Error creating image views ({}).", internal::resultString(result))
	}
}

void TextureData::createSampler() {
	const auto& vkc = internal::VulkanCore::get();
	const VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
										  .pNext = nullptr,
										  .flags = {},
										  .magFilter = VK_FILTER_LINEAR,
										  .minFilter = VK_FILTER_LINEAR,
										  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
										  .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .mipLodBias = {},
										  .anisotropyEnable = VK_TRUE,
										  .maxAnisotropy = vkc.getMaxSamplerAnisotropy(),
										  .compareEnable = VK_TRUE,
										  .compareOp = VK_COMPARE_OP_LESS,
										  .minLod = -1000,
										  .maxLod = 1000,
										  .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
										  .unnormalizedCoordinates = VK_FALSE};
	if (textureSampler != nullptr)
		vkDestroySampler(vkc.getLogicalDevice(), textureSampler, nullptr);
	if (const VkResult result = vkCreateSampler(vkc.getLogicalDevice(), &samplerInfo, nullptr, &textureSampler);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture: Error creating texture sampler ({}).", internal::resultString(result))
	}
}
void TextureData::createImage(const math::vec2ui& iDimensions) {
	freeTexture();
	const auto& vkc = internal::VulkanCore::get();
	const VkImageCreateInfo imageInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.extent = {.width = iDimensions.x(), .height = iDimensions.y(), .depth = 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT) |
					 static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
	if (const VkResult result = vkCreateImage(vkc.getLogicalDevice(), &imageInfo, nullptr, &textureImage);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture: failed to create image ({}).", internal::resultString(result))
		return;
	}
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vkc.getLogicalDevice(), textureImage, &memRequirements);
	const VkMemoryAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										 .pNext = nullptr,
										 .allocationSize = memRequirements.size,
										 .memoryTypeIndex = vkc.findMemoryTypeIndex(
												 memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
	if (const VkResult result = vkAllocateMemory(vkc.getLogicalDevice(), &allocInfo, nullptr, &textureImageMemory);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Texture: failed to allocate image memory ({}).", internal::resultString(result))
		return;
	}
}

Descriptors::Descriptors() = default;

Descriptors::~Descriptors() { release(); }

void Descriptors::release() {
	const auto& core = VulkanCore::get();
	resetTextureBind();
	m_textureBind.shrink_to_fit();
	if (!m_textures.empty()) {
		for (auto& tex: m_textures) {
			tex.second->freeTexture();
			tex.second.reset();
		}
	}
	m_textures.clear();
	if (!m_uniformBuffers.empty()) {
		for (size_t i = 0; i < g_maxFrameInFlight; ++i) {

			vkUnmapMemory(core.getLogicalDevice(), m_uniformBuffersMemory[i]);
			vkDestroyBuffer(core.getLogicalDevice(), m_uniformBuffers[i], nullptr);
			vkFreeMemory(core.getLogicalDevice(), m_uniformBuffersMemory[i], nullptr);
		}
		m_uniformBuffers.clear();
		m_uniformBuffersMemory.clear();
		m_uniformBuffersMapped.clear();
		m_uniformBuffers.shrink_to_fit();
		m_uniformBuffersMemory.shrink_to_fit();
		m_uniformBuffersMapped.shrink_to_fit();
	}
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
											  .flags = {},
											  .maxSets = g_maxFrameInFlight,
											  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
											  .pPoolSizes = poolSizes.data()};
	if (const VkResult result = vkCreateDescriptorPool(core.getLogicalDevice(), &poolInfo, nullptr, &m_descriptorPool);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Descriptors: failed to create descriptor pool ({})", resultString(result))
	}

	// Descriptor sets layout
	constexpr VkDescriptorSetLayoutBinding uboLayoutBinding{.binding = 0,
															.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
															.descriptorCount = 1,
															.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
															.pImmutableSamplers = nullptr};
	constexpr VkDescriptorSetLayoutBinding samplerLayoutBinding{.binding = 1,
																.descriptorType =
																		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
																.descriptorCount = 32,
																.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
																.pImmutableSamplers = nullptr};
	std::vector bindings = {uboLayoutBinding, samplerLayoutBinding};
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
	std::vector<VkDescriptorSetLayout> layouts(g_maxFrameInFlight, m_descriptorSetLayout);
	const VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												.pNext = nullptr,
												.descriptorPool = m_descriptorPool,
												.descriptorSetCount = g_maxFrameInFlight,
												.pSetLayouts = layouts.data()};
	m_descriptorSets.resize(g_maxFrameInFlight);
	if (const auto result = vkAllocateDescriptorSets(core.getLogicalDevice(), &allocInfo, m_descriptorSets.data());
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Descriptor: failed to allocate descriptor sets ({})", resultString(result))
	}
}

void Descriptors::updateDescriptors() {
	for (size_t i = 0; i < g_maxFrameInFlight; i++) { updateDescriptor(i); }
}

void Descriptors::updateDescriptor(const size_t iFrame) {
	const auto& core = VulkanCore::get();
	std::vector<VkDescriptorBufferInfo> bufferInfos;
	if (!m_uniformBuffers.empty()) {
		bufferInfos.push_back({.buffer = m_uniformBuffers[iFrame], .offset = 0, .range = m_uniformSize});
	}
	std::vector<VkDescriptorImageInfo> imageInfos;
	imageInfos.reserve(m_textureBind.size());
	for (const auto& id: m_textureBind) {
		imageInfos.push_back({.sampler = m_textures.getTextureData(id)->textureSampler,
							  .imageView = m_textures.getTextureData(id)->textureImageView,
							  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
	}
	std::vector<VkWriteDescriptorSet> descriptorWrites;
	if (!bufferInfos.empty()) {
		descriptorWrites.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
									.pNext = nullptr,
									.dstSet = m_descriptorSets[iFrame],
									.dstBinding = 0,
									.dstArrayElement = 0,
									.descriptorCount = static_cast<uint32_t>(bufferInfos.size()),
									.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
									.pImageInfo = nullptr,
									.pBufferInfo = bufferInfos.data(),
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

void Descriptors::registerUniform(const uint32_t iSize) {
	m_uniformBuffers.resize(g_maxFrameInFlight);
	m_uniformBuffersMemory.resize(g_maxFrameInFlight);
	m_uniformBuffersMapped.resize(g_maxFrameInFlight);
	m_uniformSize = iSize;
	const auto& core = VulkanCore::get();
	for (size_t i = 0; i < g_maxFrameInFlight; i++) {
		createBuffer(iSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffers[i],
					 m_uniformBuffersMemory[i]);
		if (const auto result = vkMapMemory(core.getLogicalDevice(), m_uniformBuffersMemory[i], 0, iSize, 0,
											&m_uniformBuffersMapped[i]);
			result != VK_SUCCESS)
			OWL_CORE_ERROR("Vulkan: Failed to create uniform buffer {}.", i)
	}
}

void Descriptors::setUniformData(const void* iData, const size_t iSize) const {
	const auto& vkh = VulkanHandler::get();
	memcpy(m_uniformBuffersMapped[vkh.getCurrentFrameIndex()], iData, iSize);
}

auto Descriptors::registerNewTexture() -> uint32_t { return m_textures.registerNewTexture(); }


auto Descriptors::isTextureRegistered(const uint32_t iIndex) const -> bool {
	if (iIndex == 0)
		return false;
	return m_textures.contains(iIndex);
}

auto Descriptors::getTextureData(const uint32_t iIndex) -> TextureData& { return *m_textures.getTextureData(iIndex); }

void Descriptors::unregisterTexture(const uint32_t iIndex) { m_textures.unregisterTexture(iIndex); }

void Descriptors::bindTextureImage(const uint32_t iIndex) {
	if (iIndex == m_bindedTexture)
		return;
	vkBindImageMemory(VulkanCore::get().getLogicalDevice(), m_textures.getTextureData(iIndex)->textureImage,
					  m_textures.getTextureData(iIndex)->textureImageMemory, 0);
	m_bindedTexture = iIndex;
}

void Descriptors::resetTextureBind() { m_textureBind.clear(); }

void Descriptors::commitTextureBind(const size_t iCurrentFrame) { updateDescriptor(iCurrentFrame); }

void Descriptors::textureBind(const uint32_t iIndex) { m_textureBind.emplace_back(iIndex); }

void Descriptors::createImguiDescriptorPool() {
	if (m_imguiDescriptorPool != nullptr)
		return;
	const auto& core = VulkanCore::get();
	// Descriptor pools.
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
											  .maxSets = g_maxFrameInFlight,
											  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
											  .pPoolSizes = poolSizes.data()};
	if (const VkResult result =
				vkCreateDescriptorPool(core.getLogicalDevice(), &poolInfo, nullptr, &m_imguiDescriptorPool);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Descriptors: failed to create descriptor pool ({})", resultString(result))
	}
}

void Descriptors::createSingleImageDescriptorPool() {
	if (m_singleImageDescriptorPool != nullptr)
		return;
	const auto& core = VulkanCore::get();
	// Descriptor pools.
	std::vector<VkDescriptorPoolSize> poolSizes{
			{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000},
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
		OWL_CORE_ERROR("Vulkan Descriptors: failed to create descriptor pool ({})", resultString(result))
	}
}
auto Descriptors::TextureList::contains(uint32_t iIndex) const -> bool {
	return std::find_if(textures.begin(), textures.end(),
						[&iIndex](const auto& iElem) { return iElem.first == iIndex; }) != textures.end();
}
auto Descriptors::TextureList::registerNewTexture() -> uint32_t {
	++nextId;
	textures.emplace_back(nextId, mkShared<TextureData>());
	return nextId;
}
void Descriptors::TextureList::unregisterTexture(uint32_t iIndex) {
	auto iter = std::find_if(textures.begin(), textures.end(),
							 [&iIndex](const auto& iElem) { return iElem.first == iIndex; });
	if (iter == textures.end())
		return;
	iter->second->freeTexture();
	textures.erase(iter);
}
auto Descriptors::TextureList::getTextureData(uint32_t iIndex) -> Descriptors::TextureList::tex {
	auto iter = std::find_if(textures.begin(), textures.end(),
							 [&iIndex](const auto& iElem) { return iElem.first == iIndex; });
	if (iter == textures.end())
		return nullptr;
	return iter->second;
}

}// namespace owl::renderer::vulkan::internal
