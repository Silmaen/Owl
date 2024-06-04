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

#include <cstddef>

namespace owl::renderer::vulkan {

namespace {

void createImage(const uint32_t iIndex, const math::FrameSize &iDimensions) {
	auto &data = internal::Descriptors::get().getTextureData(iIndex);
	data.createImage(iDimensions);
	internal::Descriptors::get().bindTextureImage(iIndex);
}

}// namespace

Texture2D::Texture2D(const math::FrameSize &iSize, const bool iWithAlpha) : renderer::Texture2D{iSize, iWithAlpha} {}

Texture2D::Texture2D(const uint32_t iWidth, const uint32_t iHeight, const bool iWithAlpha)
	: renderer::Texture2D{iWidth, iHeight, iWithAlpha} {}

Texture2D::Texture2D(std::filesystem::path iPath) : renderer::Texture2D{std::move(iPath)} {
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc *data = nullptr;
	{
		OWL_PROFILE_SCOPE("stbi_load - vulkan::Texture2D::Texture2D(const std::filesystem::path &)")
		data = stbi_load(m_path.string().c_str(), &width, &height, &channels, 0);
	}
	if (!data) {
		OWL_CORE_WARN("Vulkan Texture: Failed to load image {}", m_path.string())
		return;
	}

	if ((channels != 4) && (channels != 3)) {
		OWL_CORE_ERROR("Vulkan Texture: Impossible to load {}, invalid number of channels {}: must be 3 or 4.")
		return;
	}
	m_hasAlpha = channels == 4;
	m_size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	setData(data, m_size.surface() * static_cast<uint32_t>(channels));

	stbi_image_free(data);
}

Texture2D::~Texture2D() {
	if (m_textureId)
		internal::Descriptors::get().unregisterTexture(m_textureId);
}

bool Texture2D::operator==(const Texture &iOther) const {
	const auto &bob = dynamic_cast<const Texture2D &>(iOther);
	return bob.m_textureId == m_textureId;
}

void Texture2D::bind(uint32_t) const { internal::Descriptors::get().textureBind(m_textureId); }

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void Texture2D::setData(void *iData, const uint32_t iSize) {
	const auto &vkc = internal::VulkanCore::get();
	if (const uint32_t expected = m_hasAlpha ? m_size.surface() * 4 : m_size.surface() * 3; iSize != expected) {
		OWL_CORE_ERROR("Vulkan Texture {}: Image size missmatch: expect {}, got {}", m_path.string(), expected, iSize)
		return;
	}
	VkBuffer stagingBuffer = nullptr;
	VkDeviceMemory stagingBufferMemory = nullptr;

	const VkDeviceSize imageSize = m_size.surface() * 4ull;
	internal::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
						   stagingBufferMemory);
	void *dataPixel = nullptr;
	vkMapMemory(vkc.getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &dataPixel);
	if (m_hasAlpha) {
		// input data already in the right format, just copy
		memcpy(dataPixel, iData, imageSize);
	} else {
		// need to insert alpha chanel.
		const auto *dataChar = static_cast<uint8_t *>(iData);
		auto *dataPixelChar = static_cast<uint8_t *>(dataPixel);
		for (uint32_t i = 0, j = 0; j < iSize; i += 4, j += 3) {
			memcpy(dataPixelChar + i, dataChar + j, 3);
			*(dataPixelChar + i + 3) = 0xFFu;
		}
	}
	vkUnmapMemory(vkc.getLogicalDevice(), stagingBufferMemory);
	auto &vkd = internal::Descriptors::get();
	if (!vkd.isTextureRegistered(m_textureId)) {
		m_textureId = vkd.registerNewTexture();
		createImage(m_textureId, m_size);
	}
	auto &data = vkd.getTextureData(m_textureId);
	internal::transitionImageLayout(data.textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	internal::copyBufferToImage(stagingBuffer, data.textureImage, m_size);
	internal::transitionImageLayout(data.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	internal::freeBuffer(vkc.getLogicalDevice(), stagingBuffer, stagingBufferMemory);
	if (!data.textureImageView)
		data.createView();
	if (!data.textureSampler)
		data.createSampler();
}
OWL_DIAG_POP

uint64_t Texture2D::getRendererId() const {
	auto &desc = internal::Descriptors::get();
	auto &texData = desc.getTextureData(m_textureId);
	if (texData.textureDescriptorSet == nullptr)
		texData.createDescriptorSet();
	return reinterpret_cast<uint64_t>(texData.textureDescriptorSet);
}

}// namespace owl::renderer::vulkan
