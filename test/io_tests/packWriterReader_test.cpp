/**
 * @file packWriterReader_test.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <io/pack/PackReader.h>
#include <io/pack/PackWriter.h>

using namespace owl::io::pack;

namespace {
auto getTempDir() -> std::filesystem::path {
	auto dir = std::filesystem::temp_directory_path() / "owl_pack_tests";
	std::filesystem::create_directories(dir);
	return dir;
}
}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class PackWriterReaderTest : public ::testing::Test {
protected:
	void SetUp() override { m_tempDir = getTempDir(); }
	void TearDown() override { std::filesystem::remove_all(m_tempDir); }
	std::filesystem::path m_tempDir;
};
OWL_DIAG_POP

TEST_F(PackWriterReaderTest, round_trip_compressed_obfuscated) {
	const auto packPath = m_tempDir / "test.owlpack";

	// Prepare test data.
	const std::string sceneYaml = "Scene: test\nEntities:\n  - Entity: 123\n    Tag:\n      tag: Player\n";
	const std::vector<uint8_t> sceneData(sceneYaml.begin(), sceneYaml.end());

	const std::string textureContent(4096, '\xAB');
	const std::vector<uint8_t> textureData(textureContent.begin(), textureContent.end());

	// Write.
	PackWriter writer;
	writer.addData(sceneData, "scenes/test.owl", AssetType::Scene);
	writer.addData(textureData, "textures/hero.png", AssetType::Texture);
	EXPECT_EQ(writer.entryCount(), 2u);
	ASSERT_TRUE(writer.write(packPath, PackFlags::Default));

	// Read back.
	PackReader reader;
	ASSERT_TRUE(reader.open(packPath));
	EXPECT_TRUE(reader.isOpen());

	EXPECT_TRUE(reader.contains("scenes/test.owl"));
	EXPECT_TRUE(reader.contains("textures/hero.png"));
	EXPECT_FALSE(reader.contains("nonexistent.txt"));

	auto readScene = reader.readEntry("scenes/test.owl");
	ASSERT_TRUE(readScene.has_value());
	EXPECT_EQ(readScene.value(), sceneData);

	auto readTexture = reader.readEntry("textures/hero.png");
	ASSERT_TRUE(readTexture.has_value());
	EXPECT_EQ(readTexture.value(), textureData);

	auto allEntries = reader.listEntries();
	EXPECT_EQ(allEntries.size(), 2u);

	auto sceneEntries = reader.listEntries(AssetType::Scene);
	EXPECT_EQ(sceneEntries.size(), 1u);
	EXPECT_EQ(sceneEntries[0], "scenes/test.owl");

	auto textureEntries = reader.listEntries(AssetType::Texture);
	EXPECT_EQ(textureEntries.size(), 1u);

	reader.close();
	EXPECT_FALSE(reader.isOpen());
}

TEST_F(PackWriterReaderTest, round_trip_no_compression) {
	const auto packPath = m_tempDir / "test_none.owlpack";

	const std::string data = "Simple test data for pack without compression.";
	const std::vector<uint8_t> rawData(data.begin(), data.end());

	PackWriter writer;
	writer.addData(rawData, "data/test.txt", AssetType::Other);
	ASSERT_TRUE(writer.write(packPath, PackFlags::None));

	PackReader reader;
	ASSERT_TRUE(reader.open(packPath));

	auto result = reader.readEntry("data/test.txt");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), rawData);
}

TEST_F(PackWriterReaderTest, round_trip_compressed_only) {
	const auto packPath = m_tempDir / "test_compressed.owlpack";

	const std::string data = "Compressed but not obfuscated data for testing.";
	const std::vector<uint8_t> rawData(data.begin(), data.end());

	PackWriter writer;
	writer.addData(rawData, "data/test.txt", AssetType::Other);
	ASSERT_TRUE(writer.write(packPath, PackFlags::Compressed));

	PackReader reader;
	ASSERT_TRUE(reader.open(packPath));

	auto result = reader.readEntry("data/test.txt");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), rawData);
}

TEST_F(PackWriterReaderTest, round_trip_obfuscated_only) {
	const auto packPath = m_tempDir / "test_obfuscated.owlpack";

	const std::string data = "Obfuscated but not compressed data for testing.";
	const std::vector<uint8_t> rawData(data.begin(), data.end());

	PackWriter writer;
	writer.addData(rawData, "data/test.txt", AssetType::Other);
	ASSERT_TRUE(writer.write(packPath, PackFlags::Obfuscated));

	PackReader reader;
	ASSERT_TRUE(reader.open(packPath));

	auto result = reader.readEntry("data/test.txt");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), rawData);
}

TEST_F(PackWriterReaderTest, file_round_trip) {
	const auto packPath = m_tempDir / "test_file.owlpack";
	const auto srcFile = m_tempDir / "source.txt";

	// Create a source file.
	const std::string content = "File content for packing test.\nLine 2.\nLine 3.\n";
	{
		std::ofstream out(srcFile, std::ios::binary);
		out.write(content.data(), static_cast<std::streamsize>(content.size()));
	}

	PackWriter writer;
	writer.addFile(srcFile, "assets/source.txt", AssetType::Other);
	ASSERT_TRUE(writer.write(packPath));

	PackReader reader;
	ASSERT_TRUE(reader.open(packPath));

	auto result = reader.readEntry("assets/source.txt");
	ASSERT_TRUE(result.has_value());
	const std::string resultStr(result.value().begin(), result.value().end());
	EXPECT_EQ(resultStr, content);
}

TEST_F(PackWriterReaderTest, many_entries) {
	const auto packPath = m_tempDir / "test_many.owlpack";

	PackWriter writer;
	constexpr size_t count = 100;
	for (size_t i = 0; i < count; ++i) {
		const auto name = std::format("entry_{:03d}.dat", i);
		const auto data = std::format("Data for entry {}", i);
		const std::vector<uint8_t> rawData(data.begin(), data.end());
		writer.addData(rawData, name, AssetType::Other);
	}
	ASSERT_TRUE(writer.write(packPath));

	PackReader reader;
	ASSERT_TRUE(reader.open(packPath));

	auto allEntries = reader.listEntries();
	EXPECT_EQ(allEntries.size(), count);

	for (size_t i = 0; i < count; ++i) {
		const auto name = std::format("entry_{:03d}.dat", i);
		const auto expected = std::format("Data for entry {}", i);
		auto result = reader.readEntry(name);
		ASSERT_TRUE(result.has_value()) << "Failed to read entry: " << name;
		const std::string resultStr(result.value().begin(), result.value().end());
		EXPECT_EQ(resultStr, expected);
	}
}

TEST_F(PackWriterReaderTest, empty_pack) {
	const auto packPath = m_tempDir / "test_empty.owlpack";

	PackWriter writer;
	EXPECT_EQ(writer.entryCount(), 0u);
	ASSERT_TRUE(writer.write(packPath));

	PackReader reader;
	ASSERT_TRUE(reader.open(packPath));
	EXPECT_EQ(reader.listEntries().size(), 0u);
}

TEST_F(PackWriterReaderTest, open_nonexistent) {
	PackReader reader;
	EXPECT_FALSE(reader.open(m_tempDir / "nonexistent.owlpack"));
	EXPECT_FALSE(reader.isOpen());
}

TEST_F(PackWriterReaderTest, open_invalid_file) {
	const auto badFile = m_tempDir / "bad.owlpack";
	{
		std::ofstream out(badFile, std::ios::binary);
		out << "This is not a valid pack file.";
	}

	PackReader reader;
	EXPECT_FALSE(reader.open(badFile));
	EXPECT_FALSE(reader.isOpen());
}

TEST_F(PackWriterReaderTest, read_nonexistent_entry) {
	const auto packPath = m_tempDir / "test_missing.owlpack";

	PackWriter writer;
	const std::vector<uint8_t> data = {1, 2, 3};
	writer.addData(data, "exists.dat", AssetType::Other);
	ASSERT_TRUE(writer.write(packPath));

	PackReader reader;
	ASSERT_TRUE(reader.open(packPath));
	auto result = reader.readEntry("does_not_exist.dat");
	EXPECT_FALSE(result.has_value());
}

TEST_F(PackWriterReaderTest, writer_clear) {
	PackWriter writer;
	writer.addData({1, 2, 3}, "test.dat", AssetType::Other);
	EXPECT_EQ(writer.entryCount(), 1u);
	writer.clear();
	EXPECT_EQ(writer.entryCount(), 0u);
}

TEST_F(PackWriterReaderTest, add_nonexistent_file) {
	PackWriter writer;
	writer.addFile(m_tempDir / "does_not_exist.txt", "ghost.txt", AssetType::Other);
	EXPECT_EQ(writer.entryCount(), 0u);
}

TEST_F(PackWriterReaderTest, binary_not_readable) {
	const auto packPath = m_tempDir / "test_opaque.owlpack";

	const std::string secret = "This should not be visible in the pack file.";
	const std::vector<uint8_t> data(secret.begin(), secret.end());

	PackWriter writer;
	writer.addData(data, "secret.txt", AssetType::Other);
	ASSERT_TRUE(writer.write(packPath, PackFlags::Default));

	// Read raw file content and verify the secret string is not present.
	std::ifstream raw(packPath, std::ios::binary | std::ios::ate);
	const auto size = static_cast<size_t>(raw.tellg());
	raw.seekg(0);
	std::string rawContent(size, '\0');
	raw.read(rawContent.data(), static_cast<std::streamsize>(size));

	EXPECT_EQ(rawContent.find(secret), std::string::npos);
}
