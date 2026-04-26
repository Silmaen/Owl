/**
 * @file TextureDecoder_test.cpp
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include "renderer/TextureDecoder.h"

#include <fstream>

using namespace owl::renderer;

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
