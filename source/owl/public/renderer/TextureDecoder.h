/**
 * @file TextureDecoder.h
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "gpu/Texture.h"
#include "math/vectors.h"

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace owl::renderer {
/**
 * @brief
 *  Result of decoding an image buffer or file on the CPU.
 *
 * The decoder runs without any GPU context, so a `DecodedImage` is safe to
 * produce from a worker thread and hand back to the main thread for upload.
 */
struct OWL_API DecodedImage {
	/// Width and height in pixels; zero on failure.
	math::vec2ui size{0, 0};
	/// Pixel format deduced from the number of channels (`Rgb8` / `Rgba8`).
	gpu::ImageFormat format{gpu::ImageFormat::Rgba8};
	/// Raw tightly packed pixel data; empty on failure.
	std::vector<uint8_t> pixels;
	/// True when decoding succeeded; callers should gate any usage on this flag.
	bool valid{false};
};

/**
 * @brief
 *  Read only the header of an in-memory image buffer to extract its dimensions.
 * @param[in] iBytes Raw image bytes (PNG, JPG, ...).
 * @return Size in pixels, or `std::nullopt` if the header cannot be parsed.
 *
 * Cheap (microseconds): does not decode pixel data. Intended to size a GPU
 * texture up front so the full decode can complete later via `setData()`.
 */
OWL_API auto peekImageSize(std::span<const uint8_t> iBytes) -> std::optional<math::vec2ui>;

/**
 * @brief
 *  Decode an image from an in-memory byte buffer.
 * @param[in] iBytes Raw image bytes (PNG, JPG, ...).
 * @param[in] iDesiredChannels 0 = native channel count; 3 forces Rgb8; 4 forces Rgba8 (padding alpha when needed).
 * @return Decoded image; `valid == false` on failure.
 *
 * Thread-safe: internally sets the per-thread vertical flip flag before each
 * decode, so multiple workers can decode concurrently without races.
 */
OWL_API auto decodeImageBytes(std::span<const uint8_t> iBytes, int iDesiredChannels = 0) -> DecodedImage;

/**
 * @brief
 *  Decode an image from a file on disk.
 * @param[in] iPath Path to the image file.
 * @param[in] iDesiredChannels 0 = native channel count; 3 forces Rgb8; 4 forces Rgba8.
 * @return Decoded image; `valid == false` on failure (missing file, bad format, unsupported channel count).
 */
OWL_API auto decodeImageFile(const std::filesystem::path& iPath, int iDesiredChannels = 0) -> DecodedImage;

}// namespace owl::renderer
