/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"

#include "internal/Descriptors.h"
#include "internal/VulkanHandler.h"
#include "internal/utils.h"

#include <stb_image.h>

namespace owl::renderer::vulkan {

namespace {

void createImage(const uint32_t iIndex, const math::vec2ui& iDimensions) {
	auto& data = internal::Descriptors::get().getTextureData(iIndex);
	data.createImage(iDimensions);
	internal::Descriptors::get().bindTextureImage(iIndex);
}

}// namespace

Texture2D::Texture2D(const Specification& iSpecs) : renderer::Texture2D{iSpecs} {}

Texture2D::Texture2D(std::filesystem::path iPath) : renderer::Texture2D{std::move(iPath)} {
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc* data = nullptr;
	{
		OWL_PROFILE_SCOPE("stbi_load - vulkan::Texture2D::Texture2D(const std::filesystem::path &)")
		data = stbi_load(m_path.string().c_str(), &width, &height, &channels, 0);
	}
	if (data == nullptr) {
		OWL_CORE_WARN("Vulkan Texture: Failed to load image {}", m_path.string())
		return;
	}

	if ((channels != 4) && (channels != 3)) {
		OWL_CORE_ERROR("Vulkan Texture: Impossible to load {}, invalid number of channels {}: must be 3 or 4.")
		return;
	}
	m_specification.format = channels == 4 ? ImageFormat::Rgba8 : ImageFormat::Rgb8;
	m_specification.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	setData(data, m_specification.size.surface() * static_cast<uint32_t>(channels));

	stbi_image_free(data);
}

Texture2D::~Texture2D() {
	if (m_textureId > 0)
		internal::Descriptors::get().unregisterTexture(m_textureId);
	else
		OWL_CORE_WARN("Destroying text with null id.")
}

auto Texture2D::operator==(const Texture& iOther) const -> bool {
	const auto& bob = dynamic_cast<const Texture2D&>(iOther);
	return bob.m_textureId == m_textureId;
}

void Texture2D::bind(uint32_t) const { internal::Descriptors::get().textureBind(m_textureId); }

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void Texture2D::setData(void* iData, const uint32_t iSize) {
	const auto& vkc = internal::VulkanCore::get();
	if (const uint32_t expected = m_specification.getPixelSize() * m_specification.size.surface(); iSize != expected) {
		OWL_CORE_ERROR("Vulkan Texture {}: Image size missmatch: expect {}, got {}", m_path.string(), expected, iSize)
		return;
	}
	VkBuffer stagingBuffer = nullptr;
	VkDeviceMemory stagingBufferMemory = nullptr;

	const VkDeviceSize imageSize = m_specification.size.surface() * 4ull;
	internal::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
						   stagingBufferMemory);
	void* dataPixel = nullptr;
	vkMapMemory(vkc.getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &dataPixel);
	if (m_specification.format == ImageFormat::Rgba8) {
		// input data already in the right format, just copy
		memcpy(dataPixel, iData, imageSize);
	} else if (m_specification.format == ImageFormat::Rgb8) {
		// need to insert alpha chanel.
		const auto* dataChar = static_cast<uint8_t*>(iData);
		auto* dataPixelChar = static_cast<uint8_t*>(dataPixel);
		for (uint32_t i = 0, j = 0; j < iSize; i += 4, j += 3) {
			memcpy(dataPixelChar + i, dataChar + j, 3);
			*(dataPixelChar + i + 3) = 0xFFu;
		}
	} else {
		OWL_CORE_ERROR("Vulkan Texture, image format {} not supported.", magic_enum::enum_name(m_specification.format))
		return;
	}
	vkUnmapMemory(vkc.getLogicalDevice(), stagingBufferMemory);
	auto& vkd = internal::Descriptors::get();
	if (!vkd.isTextureRegistered(m_textureId)) {
		m_textureId = vkd.registerNewTexture();
		createImage(m_textureId, m_specification.size);
	}
	auto& data = vkd.getTextureData(m_textureId);
	internal::transitionImageLayout(data.textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	internal::copyBufferToImage(stagingBuffer, data.textureImage, m_specification.size);
	internal::transitionImageLayout(data.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	internal::freeBuffer(vkc.getLogicalDevice(), stagingBuffer, stagingBufferMemory);
	if (data.textureImageView == nullptr)
		data.createView();
	if (data.textureSampler == nullptr)
		data.createSampler();
}
OWL_DIAG_POP

auto Texture2D::getRendererId() const -> uint64_t {
	auto& desc = internal::Descriptors::get();
	auto& texData = desc.getTextureData(m_textureId);
	if (texData.textureDescriptorSet == nullptr)
		texData.createDescriptorSet();
	return reinterpret_cast<uint64_t>(texData.textureDescriptorSet);
}

}// namespace owl::renderer::vulkan
