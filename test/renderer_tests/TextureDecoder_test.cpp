/**
 * @file TextureDecoder_test.cpp
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include "renderer/TextureDecoder.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace owl::renderer;
using namespace owl::renderer::gpu;

namespace {
auto readFileAsBytes(const std::filesystem::path& iPath) -> std::vector<uint8_t> {
	std::ifstream in(iPath, std::ios::binary | std::ios::ate);
	const auto size = in.tellg();
	in.seekg(0);
	std::vector<uint8_t> bytes(static_cast<size_t>(size));
	in.read(reinterpret_cast<char*>(bytes.data()), size);
	return bytes;
}

auto getFixturePath() -> std::filesystem::path {
	return owl::test::getRootPath() / "engine_assets" / "textures" / "mario.png";
}

}// namespace

TEST(TextureDecoder, PeekImageSizeReadsPngHeader) {
	const auto bytes = readFileAsBytes(getFixturePath());
	ASSERT_FALSE(bytes.empty());
	const auto size = peekImageSize(bytes);
	ASSERT_TRUE(size.has_value());
	EXPECT_GT(size->x(), 0u);
	EXPECT_GT(size->y(), 0u);
}

TEST(TextureDecoder, PeekImageSizeRejectsEmpty) {
	const auto size = peekImageSize({});
	EXPECT_FALSE(size.has_value());
}

TEST(TextureDecoder, PeekImageSizeRejectsGarbage) {
	const std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
	const auto size = peekImageSize(garbage);
	EXPECT_FALSE(size.has_value());
}

TEST(TextureDecoder, DecodeImageFileSucceeds) {
	const auto decoded = decodeImageFile(getFixturePath());
	ASSERT_TRUE(decoded.valid);
	EXPECT_GT(decoded.size.x(), 0u);
	EXPECT_GT(decoded.size.y(), 0u);
	const size_t channels = decoded.format == ImageFormat::Rgba8 ? 4 : 3;
	EXPECT_EQ(decoded.pixels.size(), static_cast<size_t>(decoded.size.x()) * decoded.size.y() * channels);
}

TEST(TextureDecoder, DecodeImageBytesMatchesDecodeImageFile) {
	const auto bytes = readFileAsBytes(getFixturePath());
	const auto fromFile = decodeImageFile(getFixturePath());
	const auto fromBytes = decodeImageBytes(bytes);
	ASSERT_TRUE(fromFile.valid);
	ASSERT_TRUE(fromBytes.valid);
	EXPECT_EQ(fromFile.size.x(), fromBytes.size.x());
	EXPECT_EQ(fromFile.size.y(), fromBytes.size.y());
	EXPECT_EQ(fromFile.format, fromBytes.format);
	EXPECT_EQ(fromFile.pixels, fromBytes.pixels);
}

TEST(TextureDecoder, DecodeForcesRgba8WhenRequested) {
	const auto bytes = readFileAsBytes(getFixturePath());
	const auto forced = decodeImageBytes(bytes, 4);
	ASSERT_TRUE(forced.valid);
	EXPECT_EQ(forced.format, ImageFormat::Rgba8);
	EXPECT_EQ(forced.pixels.size(), static_cast<size_t>(forced.size.x()) * forced.size.y() * 4u);
}

TEST(TextureDecoder, DecodeInvalidBytesReturnsInvalid) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const std::vector<uint8_t> garbage(128, 0x42);
	const auto decoded = decodeImageBytes(garbage);
	EXPECT_FALSE(decoded.valid);
	EXPECT_TRUE(decoded.pixels.empty());
	owl::core::Log::invalidate();
}

TEST(TextureDecoder, DecodeEmptyBytesReturnsInvalid) {
	const auto decoded = decodeImageBytes({});
	EXPECT_FALSE(decoded.valid);
	EXPECT_TRUE(decoded.pixels.empty());
}

TEST(TextureDecoder, DecodeMissingFileReturnsInvalid) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto decoded = decodeImageFile("/definitely/does/not/exist.png");
	EXPECT_FALSE(decoded.valid);
	EXPECT_TRUE(decoded.pixels.empty());
	owl::core::Log::invalidate();
}

TEST(TextureDecoder, WriteImagePngRoundTrips) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto file = std::filesystem::temp_directory_path() / "owl_write_image_png_test.png";
	std::filesystem::remove(file);
	std::vector<uint8_t> pixels(3u * 2u * 4u);
	for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<uint8_t>(i * 10);
	ASSERT_TRUE(writeImagePng(file, {3, 2}, pixels));
	const auto decoded = decodeImageFile(file, 4);
	ASSERT_TRUE(decoded.valid);
	EXPECT_EQ(decoded.size.x(), 3u);
	EXPECT_EQ(decoded.size.y(), 2u);
	// The decoder returns the bottom row first (OpenGL convention): compare with the rows swapped.
	std::vector<uint8_t> bottomFirst(pixels.begin() + 12, pixels.end());
	bottomFirst.insert(bottomFirst.end(), pixels.begin(), pixels.begin() + 12);
	EXPECT_EQ(decoded.pixels, bottomFirst);
	std::filesystem::remove(file);
	owl::core::Log::invalidate();
}

TEST(TextureDecoder, WriteImagePngRejectsBadInput) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto file = std::filesystem::temp_directory_path() / "owl_write_image_png_bad.png";
	const std::vector<uint8_t> pixels(5, 0);
	EXPECT_FALSE(writeImagePng(file, {3, 2}, pixels));
	EXPECT_FALSE(writeImagePng(file, {0, 0}, {}));
	EXPECT_FALSE(writeImagePng(std::filesystem::temp_directory_path() / "owl_no_such_dir__" / "x.png", {1, 1},
							   std::vector<uint8_t>(4, 0)));
	EXPECT_FALSE(exists(file));
	owl::core::Log::invalidate();
}
