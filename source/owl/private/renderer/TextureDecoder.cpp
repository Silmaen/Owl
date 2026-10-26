/**
 * @file TextureDecoder.cpp
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/TextureDecoder.h"

#include "core/external/ktx.h"
#include "renderer/gpu/RenderCommand.h"

#include <png.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace owl::renderer {

namespace {

constexpr std::array<uint8_t, 12> g_ktx2Identifier{0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
												   0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr size_t g_ktx2WidthOffset = 20;
constexpr size_t g_ktx2HeaderSize = 28;

auto readUint32(const std::span<const uint8_t> iBytes, const size_t iOffset) -> uint32_t {
	uint32_t value = 0;
	std::memcpy(&value, iBytes.data() + iOffset, sizeof(value));
	return value;
}

auto makeDecodedImage(stbi_uc* iData, int iWidth, int iHeight, int iChannels) -> DecodedImage {
	DecodedImage out;
	if (iData == nullptr) {
		return out;
	}
	if (iChannels != 3 && iChannels != 4) {
		OWL_CORE_ERROR("TextureDecoder: unsupported channel count {} (expected 3 or 4).", iChannels)
		stbi_image_free(iData);
		return out;
	}
	out.size = {static_cast<uint32_t>(iWidth), static_cast<uint32_t>(iHeight)};
	out.format = iChannels == 4 ? gpu::ImageFormat::Rgba8 : gpu::ImageFormat::Rgb8;
	const auto byteCount = static_cast<size_t>(iWidth) * static_cast<size_t>(iHeight) * static_cast<size_t>(iChannels);
	out.pixels.assign(iData, iData + byteCount);
	stbi_image_free(iData);
	out.valid = true;
	return out;
}

}// namespace

auto peekImageSize(std::span<const uint8_t> iBytes) -> std::optional<math::vec2ui> {
	if (iBytes.empty()) {
		return std::nullopt;
	}
	if (isKtx2(iBytes)) {
		if (iBytes.size() < g_ktx2HeaderSize)
			return std::nullopt;
		return math::vec2ui{readUint32(iBytes, g_ktx2WidthOffset), readUint32(iBytes, g_ktx2WidthOffset + 4)};
	}
	int width = 0;
	int height = 0;
	int channels = 0;
	if (stbi_info_from_memory(iBytes.data(), static_cast<int>(iBytes.size()), &width, &height, &channels) == 0) {
		return std::nullopt;
	}
	return math::vec2ui{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

auto decodeImageBytes(std::span<const uint8_t> iBytes, int iDesiredChannels) -> DecodedImage {
	OWL_PROFILE_FUNCTION()

	if (iBytes.empty()) {
		return {};
	}
	// Per-thread flip state so concurrent decodes on worker threads are race-free.
	stbi_set_flip_vertically_on_load_thread(1);
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* data = stbi_load_from_memory(iBytes.data(), static_cast<int>(iBytes.size()), &width, &height, &channels,
										  iDesiredChannels);
	const int effectiveChannels = iDesiredChannels == 0 ? channels : iDesiredChannels;
	return makeDecodedImage(data, width, height, effectiveChannels);
}

auto decodeImageFile(const std::filesystem::path& iPath, int iDesiredChannels) -> DecodedImage {
	OWL_PROFILE_FUNCTION()

	stbi_set_flip_vertically_on_load_thread(1);
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* data = stbi_load(iPath.string().c_str(), &width, &height, &channels, iDesiredChannels);
	if (data == nullptr) {
		OWL_CORE_WARN("TextureDecoder: failed to load image {}.", iPath.string())
		return {};
	}
	const int effectiveChannels = iDesiredChannels == 0 ? channels : iDesiredChannels;
	return makeDecodedImage(data, width, height, effectiveChannels);
}

auto writeImagePng(const std::filesystem::path& iPath, const math::vec2ui iSize, const std::span<const uint8_t> iRgba)
		-> bool {
	if (iSize.surface() == 0 || iRgba.size() != static_cast<size_t>(iSize.surface()) * 4) {
		OWL_CORE_ERROR("TextureDecoder: Cannot write {} ({}x{} image, {} bytes).", iPath.string(), iSize.x(), iSize.y(),
					   iRgba.size())
		return false;
	}
	png_image image{};
	image.version = PNG_IMAGE_VERSION;
	image.width = iSize.x();
	image.height = iSize.y();
	image.format = PNG_FORMAT_RGBA;
	if (png_image_write_to_file(&image, iPath.string().c_str(), 0, iRgba.data(), 0, nullptr) == 0) {
		OWL_CORE_ERROR("TextureDecoder: Failed to write {} ({}).", iPath.string(), image.message)
		png_image_free(&image);
		return false;
	}
	return true;
}

auto encodeImagePng(const math::vec2ui iSize, const std::span<const uint8_t> iRgba) -> std::vector<uint8_t> {
	if (iSize.surface() == 0 || iRgba.size() != static_cast<size_t>(iSize.surface()) * 4) {
		OWL_CORE_ERROR("TextureDecoder: Cannot encode a {}x{} image from {} bytes.", iSize.x(), iSize.y(), iRgba.size())
		return {};
	}
	png_image image{};
	image.version = PNG_IMAGE_VERSION;
	image.width = iSize.x();
	image.height = iSize.y();
	image.format = PNG_FORMAT_RGBA;
	png_alloc_size_t bytes = 0;
	if (png_image_write_to_memory(&image, nullptr, &bytes, 0, iRgba.data(), 0, nullptr) == 0) {
		OWL_CORE_ERROR("TextureDecoder: Cannot size the PNG of a {}x{} image ({}).", iSize.x(), iSize.y(),
					   image.message)
		png_image_free(&image);
		return {};
	}
	std::vector<uint8_t> png(bytes);
	if (png_image_write_to_memory(&image, png.data(), &bytes, 0, iRgba.data(), 0, nullptr) == 0) {
		OWL_CORE_ERROR("TextureDecoder: Cannot encode a {}x{} image ({}).", iSize.x(), iSize.y(), image.message)
		png_image_free(&image);
		return {};
	}
	png.resize(bytes);
	return png;
}

auto isKtx2(const std::span<const uint8_t> iBytes) -> bool {
	return iBytes.size() >= g_ktx2Identifier.size() &&
		   std::equal(g_ktx2Identifier.begin(), g_ktx2Identifier.end(), iBytes.begin());
}

auto pickKtx2Target() -> gpu::ImageFormat {
	if (gpu::RenderCommand::supportsFormat(gpu::ImageFormat::Bc7))
		return gpu::ImageFormat::Bc7;
	if (gpu::RenderCommand::supportsFormat(gpu::ImageFormat::Astc4x4))
		return gpu::ImageFormat::Astc4x4;
	return gpu::ImageFormat::Rgba8;
}

auto transcodeKtx2(const std::span<const uint8_t> iBytes, const gpu::ImageFormat iTarget)
		-> std::optional<gpu::TextureLevels> {
	OWL_PROFILE_FUNCTION()

	ktx_transcode_fmt_e target = KTX_TTF_RGBA32;
	switch (iTarget) {
		case gpu::ImageFormat::Bc7:
			target = KTX_TTF_BC7_RGBA;
			break;
		case gpu::ImageFormat::Astc4x4:
			target = KTX_TTF_ASTC_4x4_RGBA;
			break;
		case gpu::ImageFormat::Rgba8:
			break;
		case gpu::ImageFormat::None:
		case gpu::ImageFormat::R8:
		case gpu::ImageFormat::Rgb8:
		case gpu::ImageFormat::Rgba32F:
			OWL_CORE_WARN("TextureDecoder: Cannot transcode KTX2 to {}.", magic_enum::enum_name(iTarget))
			return std::nullopt;
	}
	ktxTexture2* texture = nullptr;
	if (const auto result = ktxTexture2_CreateFromMemory(iBytes.data(), iBytes.size(),
														  KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
		result != KTX_SUCCESS) {
		OWL_CORE_WARN("TextureDecoder: Unreadable KTX2 texture ({}).", ktxErrorString(result))
		return std::nullopt;
	}
	if (ktxTexture2_NeedsTranscoding(texture)) {
		if (const auto result = ktxTexture2_TranscodeBasis(texture, target, 0); result != KTX_SUCCESS) {
			OWL_CORE_WARN("TextureDecoder: Cannot transcode KTX2 texture ({}).", ktxErrorString(result))
			ktxTexture2_Destroy(texture);
			return std::nullopt;
		}
	} else if (iTarget != gpu::ImageFormat::Rgba8) {
		OWL_CORE_WARN("TextureDecoder: KTX2 texture holds no Basis Universal data.")
		ktxTexture2_Destroy(texture);
		return std::nullopt;
	}
	gpu::TextureLevels levels{.size = {texture->baseWidth, texture->baseHeight}, .format = iTarget, .levels = {}};
	auto* base = ktxTexture(texture);
	const auto* data = ktxTexture_GetData(base);
	for (ktx_uint32_t level = 0; level < texture->numLevels; ++level) {
		ktx_size_t offset = 0;
		if (ktxTexture_GetImageOffset(base, level, 0, 0, &offset) != KTX_SUCCESS)
			break;
		const auto size = ktxTexture_GetImageSize(base, level);
		levels.levels.emplace_back(data + offset, data + offset + size);
	}
	ktxTexture2_Destroy(texture);
	if (levels.levels.empty()) {
		OWL_CORE_WARN("TextureDecoder: KTX2 texture has no level.")
		return std::nullopt;
	}
	return levels;
}

}// namespace owl::renderer
