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

#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <vector>

namespace owl::renderer::gpu::vulkan {

namespace {
void createImage(const uint32_t iIndex, const math::vec2ui& iDimensions) {
	auto& data = internal::Descriptors::get().getTextureData(iIndex);
	data.createImage(iDimensions);
}

auto cubeFormat(const ImageFormat iFormat) -> VkFormat {
	switch (iFormat) {
		case ImageFormat::R32F:
			return VK_FORMAT_R32_SFLOAT;
		case ImageFormat::Rgba16F:
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		case ImageFormat::Rgba32F:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		case ImageFormat::Rgba8:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case ImageFormat::None:
		case ImageFormat::R8:
		case ImageFormat::Rgb8:
		case ImageFormat::Bc7:
		case ImageFormat::Astc4x4:
			return VK_FORMAT_UNDEFINED;
	}
	return VK_FORMAT_UNDEFINED;
}

}// namespace

Texture2D::Texture2D(const Specification& iSpecs) : renderer::gpu::Texture2D{iSpecs} {}

Texture2D::Texture2D(VkImageView iView, VkSampler iSampler, const math::vec2ui& iSize)
	: renderer::gpu::Texture2D{Specification{.size = iSize}} {
	auto& vkd = internal::Descriptors::get();
	m_textureId = vkd.registerNewTexture();
	auto& data = vkd.getTextureData(m_textureId);
	data.borrowed = true;
	data.debugName = "attachment";
	setBorrowed(iView, iSampler, iSize);
}

void Texture2D::setBorrowed(VkImageView iView, VkSampler iSampler, const math::vec2ui& iSize) {
	auto& data = internal::Descriptors::get().getTextureData(m_textureId);
	data.textureImageView = iView;
	data.textureSampler = iSampler;
	m_specification.size = iSize;
}

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
	const bool floating = m_specification.format == ImageFormat::Rgba32F;
	const VkDeviceSize imageSize = m_specification.size.surface() * (floating ? 16ull : 4ull);
	auto staging = internal::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, internal::MemoryUsage::Upload,
										  "tex.staging");
	if (staging.mapped == nullptr)
		return;
	if (m_specification.format == ImageFormat::Rgba8 || floating) {
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
		texData.filter = m_specification.filterMode;
		texData.format = floating ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
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
auto Texture2D::setLevels(const TextureLevels& iLevels) -> bool {
	if (iLevels.levels.empty() || iLevels.size.surface() == 0) {
		OWL_CORE_WARN("Vulkan Texture: No level to set on '{}'.", getName())
		return false;
	}
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	switch (iLevels.format) {
		case ImageFormat::Bc7:
			format = VK_FORMAT_BC7_UNORM_BLOCK;
			break;
		case ImageFormat::Astc4x4:
			format = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
			break;
		case ImageFormat::Rgba8:
			break;
		case ImageFormat::None:
		case ImageFormat::R8:
		case ImageFormat::Rgb8:
		case ImageFormat::Rgba32F:
		case ImageFormat::R32F:
		case ImageFormat::Rgba16F:
			OWL_CORE_WARN("Vulkan Texture: Levels in format {} are not supported.",
						  magic_enum::enum_name(iLevels.format))
			return false;
	}
	VkDeviceSize total = 0;
	for (const auto& level: iLevels.levels) total += level.size();
	auto staging = internal::createBuffer(total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, internal::MemoryUsage::Upload,
										  "tex.staging");
	if (staging.mapped == nullptr)
		return false;
	std::vector<VkDeviceSize> offsets;
	offsets.reserve(iLevels.levels.size());
	VkDeviceSize offset = 0;
	for (const auto& level: iLevels.levels) {
		offsets.push_back(offset);
		std::memcpy(static_cast<uint8_t*>(staging.mapped) + offset, level.data(), level.size());
		offset += level.size();
	}

	auto specs = m_specification;
	specs.size = iLevels.size;
	specs.format = iLevels.format;
	specs.levelCount = static_cast<uint32_t>(iLevels.levels.size());
	reallocate(specs);
	auto& vkd = internal::Descriptors::get();
	m_textureId = vkd.registerNewTexture();
	auto& data = vkd.getTextureData(m_textureId);
	data.debugName = !getName().empty() ? getName() : getPath().filename().string();
	data.mipLevels = specs.levelCount;
	data.filter = specs.filterMode;
	data.format = format;
	createImage(m_textureId, specs.size);
	internal::VulkanHandler::get().recordTransfer([&data, &staging, &offsets, &specs](VkCommandBuffer iCmd) -> void {
		internal::transitionImageLayout(iCmd, data.textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
										VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, data.mipLevels);
		for (uint32_t level = 0; level < data.mipLevels; ++level) {
			const math::vec2ui size{std::max(1u, specs.size.x() >> level), std::max(1u, specs.size.y() >> level)};
			internal::copyBufferToImageLevel(iCmd, staging.buffer, data.textureImage, size, level, offsets[level]);
		}
		internal::transitionImageLayout(iCmd, data.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
										VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, data.mipLevels);
	});
	internal::releaseBuffer(staging);
	data.createView();
	data.createSampler();
	return true;
}
OWL_DIAG_POP

void Texture2D::reallocate(const Specification& iSpecs) {
	if (m_textureId > 0)
		internal::Descriptors::get().unregisterTexture(m_textureId);
	m_textureId = 0;
	m_specification = iSpecs;
}

void Texture2D::setFilterMode(const FilterMode iMode) {
	m_specification.filterMode = iMode;
	auto& vkd = internal::Descriptors::get();
	if (!vkd.isTextureRegistered(m_textureId))
		return;
	auto& data = vkd.getTextureData(m_textureId);
	if (data.filter == iMode)
		return;
	data.filter = iMode;
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

TextureCube::TextureCube(const Specification& iSpecs) : renderer::gpu::TextureCube{iSpecs} {
	auto& vkd = internal::Descriptors::get();
	m_textureId = vkd.registerNewTexture();
	auto& data = vkd.getTextureData(m_textureId);
	data.debugName = "cube";
	data.cube = true;
	data.mipLevels = getLevelCount();
	data.filter = m_specification.filterMode;
	data.format = cubeFormat(m_specification.format);
	data.createImage(m_specification.size);
	internal::VulkanHandler::get().recordTransfer([&data](VkCommandBuffer iCmd) -> void {
		internal::transitionImageLayout(iCmd, data.textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
										VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, data.mipLevels, g_faceCount);
		constexpr VkClearColorValue black{.float32 = {0.f, 0.f, 0.f, 0.f}};
		const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
											.baseMipLevel = 0,
											.levelCount = data.mipLevels,
											.baseArrayLayer = 0,
											.layerCount = g_faceCount};
		vkCmdClearColorImage(iCmd, data.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
		internal::transitionImageLayout(iCmd, data.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
										VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, data.mipLevels, g_faceCount);
	});
	data.createView();
	data.createSampler();
}

TextureCube::~TextureCube() {
	if (m_textureId > 0)
		internal::Descriptors::get().unregisterTexture(m_textureId);
}

auto TextureCube::operator==(const Texture& iOther) const -> bool {
	const auto* other = dynamic_cast<const TextureCube*>(&iOther);
	return other != nullptr && other->m_textureId == m_textureId;
}

void TextureCube::bind(const uint32_t iSlot) const {
		OWL_CORE_WARN("Vulkan TextureCube: Cannot bind a cube to the 2D slot {}, bind it with the cubes.", iSlot)}

OWL_DIAG_PUSH OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage") auto TextureCube::upload(
		const std::span<const std::span<const uint8_t>> iLevels) const -> bool {
	VkDeviceSize total = 0;
	for (const auto& level: iLevels) total += level.size();
	auto staging = internal::createBuffer(total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, internal::MemoryUsage::Upload,
										  "cube.staging");
	if (staging.mapped == nullptr) {
		OWL_CORE_WARN("Vulkan TextureCube: No staging memory for {} bytes.", total)
		return false;
	}
	std::vector<VkBufferImageCopy> regions;
	regions.reserve(iLevels.size());
	VkDeviceSize offset = 0;
	for (uint32_t level = 0; level < iLevels.size(); ++level) {
		std::memcpy(static_cast<uint8_t*>(staging.mapped) + offset, iLevels[level].data(), iLevels[level].size());
		const uint32_t face = getFaceSize(level);
		regions.push_back({.bufferOffset = offset,
						   .bufferRowLength = 0,
						   .bufferImageHeight = 0,
						   .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
												.mipLevel = level,
												.baseArrayLayer = 0,
												.layerCount = g_faceCount},
						   .imageOffset = {0, 0, 0},
						   .imageExtent = {face, face, 1}});
		offset += iLevels[level].size();
	}
	auto& data = internal::Descriptors::get().getTextureData(m_textureId);
	internal::VulkanHandler::get().recordTransfer([&data, &staging, &regions](VkCommandBuffer iCmd) -> void {
		internal::transitionImageLayout(iCmd, data.textureImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
										VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, data.mipLevels, g_faceCount);
		vkCmdCopyBufferToImage(iCmd, staging.buffer, data.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							   static_cast<uint32_t>(regions.size()), regions.data());
		internal::transitionImageLayout(iCmd, data.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
										VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, data.mipLevels, g_faceCount);
	});
	internal::releaseBuffer(staging);
	return true;
}
OWL_DIAG_POP

void TextureCube::setData(void* iData, const uint32_t iSize) {
	if (iSize != m_specification.size.surface() * m_specification.getPixelSize() * g_faceCount) {
		OWL_CORE_WARN("Vulkan TextureCube: Data of {} bytes does not hold the six faces.", iSize)
		return;
	}
	const std::array levels{std::span<const uint8_t>{static_cast<const uint8_t*>(iData), iSize}};
	static_cast<void>(upload(levels));
}

auto TextureCube::setLevels(const TextureLevels& iLevels) -> bool {
	if (iLevels.size != m_specification.size || iLevels.format != m_specification.format ||
		iLevels.levels.size() != getLevelCount()) {
		OWL_CORE_WARN("Vulkan TextureCube: Levels do not match the cube.")
		return false;
	}
	std::vector<std::span<const uint8_t>> levels;
	levels.reserve(iLevels.levels.size());
	for (uint32_t level = 0; level < getLevelCount(); ++level) {
		const uint32_t face = getFaceSize(level);
		if (iLevels.levels[level].size() !=
			static_cast<size_t>(face) * face * m_specification.getPixelSize() * g_faceCount) {
			OWL_CORE_WARN("Vulkan TextureCube: Level {} does not hold the six faces.", level)
			return false;
		}
		levels.emplace_back(iLevels.levels[level]);
	}
	return upload(levels);
}

}// namespace owl::renderer::gpu::vulkan
