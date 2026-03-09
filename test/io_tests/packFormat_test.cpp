/**
 * @file packFormat_test.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <io/pack/PackFormat.h>

using namespace owl::io::pack;

TEST(PackFormat, hashPath_deterministic) {
	const auto hash1 = hashPath("textures/mario.png");
	const auto hash2 = hashPath("textures/mario.png");
	EXPECT_EQ(hash1, hash2);
}

TEST(PackFormat, hashPath_different) {
	const auto hash1 = hashPath("textures/mario.png");
	const auto hash2 = hashPath("textures/luigi.png");
	EXPECT_NE(hash1, hash2);
}

TEST(PackFormat, hashPath_empty) {
	const auto hash = hashPath("");
	EXPECT_NE(hash, 0u);
}

TEST(PackFormat, obfuscate_symmetric) {
	std::vector<uint8_t> original = {0x01, 0x02, 0x03, 0x04, 0x05, 0xAA, 0xBB, 0xCC};
	auto data = original;
	obfuscateBuffer(data, 42);
	EXPECT_NE(data, original);
	obfuscateBuffer(data, 42);
	EXPECT_EQ(data, original);
}

TEST(PackFormat, obfuscate_different_indices) {
	std::vector<uint8_t> data1 = {0x01, 0x02, 0x03, 0x04};
	std::vector<uint8_t> data2 = data1;
	obfuscateBuffer(data1, 0);
	obfuscateBuffer(data2, 1);
	EXPECT_NE(data1, data2);
}

TEST(PackFormat, obfuscate_empty) {
	std::vector<uint8_t> data;
	obfuscateBuffer(data, 0);
	EXPECT_TRUE(data.empty());
}

TEST(PackFormat, compress_decompress) {
	const std::string text = "Hello, this is a test string that should compress well. "
							 "Hello, this is a test string that should compress well. "
							 "Hello, this is a test string that should compress well.";
	const std::vector<uint8_t> original(text.begin(), text.end());

	auto compressed = compressBuffer(original);
	ASSERT_FALSE(compressed.empty());
	EXPECT_LT(compressed.size(), original.size());

	auto decompressed = decompressBuffer(compressed, original.size());
	EXPECT_EQ(decompressed, original);
}

TEST(PackFormat, compress_empty) {
	const std::vector<uint8_t> empty;
	auto compressed = compressBuffer(empty);
	EXPECT_TRUE(compressed.empty());
}

TEST(PackFormat, decompress_empty) {
	auto result = decompressBuffer({}, 0);
	EXPECT_TRUE(result.empty());
}

TEST(PackFormat, decompress_wrong_size) {
	const std::vector<uint8_t> original = {1, 2, 3, 4, 5};
	auto compressed = compressBuffer(original);
	auto result = decompressBuffer(compressed, 999);
	EXPECT_TRUE(result.empty());
}

TEST(PackFormat, toc_round_trip) {
	std::vector<TocEntry> entries;
	entries.push_back({hashPath("scenes/test.owl"), "scenes/test.owl", 32, 1024, 2048, AssetType::Scene});
	entries.push_back({hashPath("textures/hero.png"), "textures/hero.png", 1056, 4096, 8192, AssetType::Texture});
	entries.push_back({hashPath("sounds/click.wav"), "sounds/click.wav", 5152, 512, 1024, AssetType::Sound});

	auto serialized = serializeToc(entries);
	ASSERT_FALSE(serialized.empty());

	auto deserialized = deserializeToc(serialized);
	ASSERT_EQ(deserialized.size(), entries.size());

	for (size_t i = 0; i < entries.size(); ++i) {
		EXPECT_EQ(deserialized[i].pathHash, entries[i].pathHash);
		EXPECT_EQ(deserialized[i].path, entries[i].path);
		EXPECT_EQ(deserialized[i].dataOffset, entries[i].dataOffset);
		EXPECT_EQ(deserialized[i].dataSize, entries[i].dataSize);
		EXPECT_EQ(deserialized[i].originalSize, entries[i].originalSize);
		EXPECT_EQ(deserialized[i].assetType, entries[i].assetType);
	}
}

TEST(PackFormat, toc_empty) {
	std::vector<TocEntry> empty;
	auto serialized = serializeToc(empty);
	EXPECT_TRUE(serialized.empty());
	auto deserialized = deserializeToc(serialized);
	EXPECT_TRUE(deserialized.empty());
}

TEST(PackFormat, flags) {
	EXPECT_TRUE(hasFlag(PackFlags::Default, PackFlags::Compressed));
	EXPECT_TRUE(hasFlag(PackFlags::Default, PackFlags::Obfuscated));
	EXPECT_FALSE(hasFlag(PackFlags::None, PackFlags::Compressed));
	EXPECT_FALSE(hasFlag(PackFlags::Compressed, PackFlags::Obfuscated));
	EXPECT_TRUE(hasFlag(PackFlags::Compressed | PackFlags::Obfuscated, PackFlags::Compressed));
}

TEST(PackFormat, header_size) {
	EXPECT_EQ(sizeof(PackHeader), 40u);
}
