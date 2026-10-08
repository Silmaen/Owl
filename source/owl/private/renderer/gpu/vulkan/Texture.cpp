/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"

#include "internal/Descriptors.h"
#include "internal/FrameProfiler.h"
#include "internal/RendererDescriptors.h"
#include "internal/VulkanHandler.h"
#include "internal/utils.h"
#include "renderer/TextureDecoder.h"

#include <cstring>

namespace owl::renderer::gpu::vulkan {

namespace {
void createImage(const uint32_t iIndex, const math::vec2ui& iDimensions) {
	auto& data = internal::Descriptors::get().getTextureData(iIndex);
	data.createImage(iDimensions);
}

}// namespace

Texture2D::Texture2D(const Specification& iSpecs) : renderer::gpu::Texture2D{iSpecs} {}

Texture2D::Texture2D(std::filesystem::path iPath) : renderer::gpu::Texture2D{std::move(iPath)} {
	const auto decoded = decodeImageFile(m_path);
	if (!decoded.valid) {
		return;
	}
	m_specification.format = decoded.format;
	m_specification.size = decoded.size;

	setData(const_cast<uint8_t*>(decoded.pixels.data()), static_cast<uint32_t>(decoded.pixels.size()));
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

void Texture2D::bind(const uint32_t iSlot) const {
	if (auto* const rd = internal::RendererDescriptors::getActive(); rd != nullptr) {
		rd->textureBind(iSlot, m_textureId);
		return;
	}
	internal::Descriptors::get().textureBind(iSlot, m_textureId);
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void Texture2D::setData(void* iData, const uint32_t iSize) {
	if (const uint32_t expected = m_specification.getPixelSize() * m_specification.size.surface(); iSize != expected) {
		OWL_CORE_ERROR("Vulkan Texture {}: Image size mismatch: expect {}, got {}.", m_path.string(), expected, iSize)
		return;
	}
	const VkDeviceSize imageSize = m_specification.size.surface() * 4ull;
	auto staging = internal::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, internal::MemoryUsage::Upload,
										  "tex.staging");
	if (staging.mapped == nullptr)
		return;
	if (m_specification.format == ImageFormat::Rgba8) {
		// input data already in the right format, just copy
		internal::writeMapped(staging, iData, imageSize);
	} else if (m_specification.format == ImageFormat::Rgb8) {
		// need to insert alpha channel.
		const auto* dataChar = static_cast<uint8_t*>(iData);
		auto* dataPixelChar = static_cast<uint8_t*>(staging.mapped);
		for (uint32_t i = 0, j = 0; j < iSize; i += 4, j += 3) {
			memcpy(dataPixelChar + i, dataChar + j, 3);
			*(dataPixelChar + i + 3) = 0xFFu;
		}
	} else {
		OWL_CORE_ERROR("Vulkan Texture, image format {} not supported.", magic_enum::enum_name(m_specification.format))
		internal::freeBuffer(staging);
		return;
	}
	auto& vkd = internal::Descriptors::get();
	if (!vkd.isTextureRegistered(m_textureId)) {
		m_textureId = vkd.registerNewTexture();
		auto& texData = vkd.getTextureData(m_textureId);
		if (!getName().empty())
			texData.debugName = getName();
		else if (!getPath().empty())
			texData.debugName = getPath().filename().string();
		else
			texData.debugName = "anon";
		texData.mipLevels = m_specification.getMipLevelCount();
		texData.nearest = m_specification.filterMode == FilterMode::Nearest;
		createImage(m_textureId, m_specification.size);
	}
	auto& data = vkd.getTextureData(m_textureId);
	internal::VulkanHandler::get().recordTransfer([&data, &staging, this](VkCommandBuffer iCmd) -> void {
		internal::transitionImageLayout(iCmd, data.textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
										VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, data.mipLevels);
		internal::copyBufferToImage(iCmd, staging.buffer, data.textureImage, m_specification.size);
		if (data.mipLevels > 1)
			internal::generateMipmaps(iCmd, data.textureImage, m_specification.size, data.mipLevels);
		else
			internal::transitionImageLayout(iCmd, data.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
											VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});
	internal::releaseBuffer(staging);
	if (data.textureImageView == nullptr)
		data.createView();
	if (data.textureSampler == nullptr)
		data.createSampler();
}
OWL_DIAG_POP

void Texture2D::setFilterMode(const FilterMode iMode) {
	m_specification.filterMode = iMode;
	auto& vkd = internal::Descriptors::get();
	if (!vkd.isTextureRegistered(m_textureId))
		return;
	auto& data = vkd.getTextureData(m_textureId);
	if (data.nearest == (iMode == FilterMode::Nearest))
		return;
	data.nearest = iMode == FilterMode::Nearest;
	if (data.textureSampler == nullptr)
		return;
	data.createSampler();
}

auto Texture2D::getRendererId() const -> uint64_t {
	auto& desc = internal::Descriptors::get();
	auto& texData = desc.getTextureData(m_textureId);
	if (texData.textureDescriptorSet == nullptr)
		texData.createDescriptorSet();
	return reinterpret_cast<uint64_t>(texData.textureDescriptorSet);
}

}// namespace owl::renderer::gpu::vulkan
