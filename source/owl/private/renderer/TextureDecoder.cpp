/**
 * @file TextureDecoder.cpp
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/TextureDecoder.h"

#include <png.h>
#include <stb_image.h>

#include <cstdint>
#include <span>

namespace owl::renderer {

namespace {
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

}// namespace owl::renderer
