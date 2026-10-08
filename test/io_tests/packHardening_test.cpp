/**
 * @file packHardening_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Macros.h>
#include <data/assets/pack/PackExtractor.h>
#include <data/assets/pack/PackFormat.h>
#include <data/assets/pack/PackReader.h>
#include <data/assets/pack/PackWriter.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wunsafe-buffer-usage")
#include <zstd.h>
OWL_DIAG_POP

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace owl::data::assets::pack;

namespace {

// Raw ingredients of a forged pack: the data area and the TOC entries pointing into it.
struct ForgedPack {
	std::vector<uint8_t> dataArea;
	std::vector<TocEntry> entries;
	bool compressed = false;
};

auto makeEntry(const std::string& iPath, const uint64_t iOffset, const uint64_t iDataSize, const uint64_t iOriginalSize)
		-> TocEntry {
	return TocEntry{.pathHash = hashPath(iPath),
					.path = iPath,
					.dataOffset = iOffset,
					.dataSize = iDataSize,
					.originalSize = iOriginalSize,
					.assetType = AssetType::Other};
}

// Plain (uncompressed, not obfuscated) pack holding one entry with the given path.
auto plainPack(const std::string& iPath, const std::string& iContent = "payload") -> ForgedPack {
	ForgedPack pack;
	pack.dataArea.assign(iContent.begin(), iContent.end());
	pack.entries.push_back(makeEntry(iPath, sizeof(PackHeader), iContent.size(), iContent.size()));
	return pack;
}

// Write a forged pack; `iPatch` may alter the computed header before it is written.
auto writeForged(const std::filesystem::path& iFile, const ForgedPack& iPack,
				 const std::function<void(PackHeader&)>& iPatch = {}) -> void {
	auto toc = serializeToc(iPack.entries);
	PackHeader header{};
	header.flags = static_cast<uint16_t>(iPack.compressed ? PackFlags::Compressed : PackFlags::None);
	header.entryCount = static_cast<uint32_t>(iPack.entries.size());
	header.tocOffset = sizeof(PackHeader) + iPack.dataArea.size();
	header.tocOriginalSize = toc.size();
	if (iPack.compressed)
		toc = compressBuffer(toc);
	header.tocSize = toc.size();
	if (iPatch)
		iPatch(header);
	std::ofstream out(iFile, std::ios::binary);
	out.write(reinterpret_cast<const char*>(&header), sizeof(PackHeader));
	out.write(reinterpret_cast<const char*>(iPack.dataArea.data()),
			  static_cast<std::streamsize>(iPack.dataArea.size()));
	out.write(reinterpret_cast<const char*>(toc.data()), static_cast<std::streamsize>(toc.size()));
}

auto openError(const std::filesystem::path& iFile) -> std::optional<PackOpenError> {
	PackReader reader;
	const auto result = reader.tryOpen(iFile);
	EXPECT_EQ(reader.isOpen(), result.has_value());
	if (result)
		return std::nullopt;
	return result.error();
}

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class PackHardeningTest : public ::testing::Test {
protected:
	void SetUp() override {
		m_tempDir = std::filesystem::temp_directory_path() / "owl_pack_hardening_tests";
		std::filesystem::remove_all(m_tempDir);
		std::filesystem::create_directories(m_tempDir / "root");
		std::filesystem::create_directories(m_tempDir / "outside");
	}
	void TearDown() override { std::filesystem::remove_all(m_tempDir); }
	std::filesystem::path m_tempDir;
};
OWL_DIAG_POP

// --- error messages ----------------------------------------------------------

TEST(PackOpenErrorText, every_error_has_a_description_and_a_fix) {
	for (const auto error:
		 {PackOpenError::CannotOpenFile, PackOpenError::ShortHeader, PackOpenError::InvalidMagic,
		  PackOpenError::UnsupportedVersion, PackOpenError::TocReadFailed, PackOpenError::TocDecompressionFailed,
		  PackOpenError::TocSizeMismatch, PackOpenError::TocOutOfBounds, PackOpenError::TocTooLarge,
		  PackOpenError::EntryOutOfBounds, PackOpenError::EntrySizeInvalid, PackOpenError::InvalidEntry,
		  PackOpenError::UnsafeEntryPath, PackOpenError::UnexpectedException}) {
		EXPECT_FALSE(describe(error).empty());
		EXPECT_FALSE(fixHint(error).empty());
	}
	EXPECT_NE(fixHint(PackOpenError::CannotOpenFile).find("runner.yml"), std::string_view::npos);
}

// --- isSafeEntryPath ---------------------------------------------------------

TEST(PackFormatPath, accepts_relative_paths) {
	EXPECT_TRUE(isSafeEntryPath("a.txt"));
	EXPECT_TRUE(isSafeEntryPath("textures/hero.png"));
	EXPECT_TRUE(isSafeEntryPath("./scripts/x.lua"));
	EXPECT_TRUE(isSafeEntryPath("a//b"));
	EXPECT_TRUE(isSafeEntryPath("fonts\\OpenSans.ttf"));
	EXPECT_TRUE(isSafeEntryPath("..hidden/file..txt"));
}

TEST(PackFormatPath, rejects_escaping_paths) {
	EXPECT_FALSE(isSafeEntryPath(""));
	EXPECT_FALSE(isSafeEntryPath(".."));
	EXPECT_FALSE(isSafeEntryPath("../evil"));
	EXPECT_FALSE(isSafeEntryPath("a/../../evil"));
	EXPECT_FALSE(isSafeEntryPath("a/.."));
	EXPECT_FALSE(isSafeEntryPath("..\\evil"));
	EXPECT_FALSE(isSafeEntryPath("a\\..\\..\\evil"));
	EXPECT_FALSE(isSafeEntryPath("/etc/passwd"));
	EXPECT_FALSE(isSafeEntryPath("\\\\server\\share"));
	EXPECT_FALSE(isSafeEntryPath("C:\\Windows\\evil"));
	EXPECT_FALSE(isSafeEntryPath("C:evil"));
	EXPECT_FALSE(isSafeEntryPath(std::string_view{"a\0b", 3}));
}

// --- tryOpen on forged packs -------------------------------------------------

TEST_F(PackHardeningTest, forged_plain_pack_opens) {
	const auto file = m_tempDir / "plain.owlpack";
	writeForged(file, plainPack("ok.txt"));
	PackReader reader;
	ASSERT_TRUE(reader.tryOpen(file).has_value());
	const auto data = reader.readEntry("ok.txt");
	ASSERT_TRUE(data.has_value());
	EXPECT_EQ(std::string(data->begin(), data->end()), "payload");
}

TEST_F(PackHardeningTest, parent_traversal_entry_rejected) {
	for (const auto* path: {"../evil", "a/../../evil", "..\\evil"}) {
		const auto file = m_tempDir / "traversal.owlpack";
		writeForged(file, plainPack(path));
		EXPECT_EQ(openError(file), PackOpenError::UnsafeEntryPath) << path;
	}
	EXPECT_FALSE(std::filesystem::exists(m_tempDir / "evil"));
}

TEST_F(PackHardeningTest, absolute_entry_rejected) {
	const auto absolute = (m_tempDir / "outside" / "evil").string();
	for (const auto& path: {absolute, std::string{"C:evil"}, std::string{"\\evil"}}) {
		const auto file = m_tempDir / "absolute.owlpack";
		writeForged(file, plainPack(path));
		EXPECT_EQ(openError(file), PackOpenError::UnsafeEntryPath) << path;
	}
	EXPECT_FALSE(std::filesystem::exists(absolute));
}

TEST_F(PackHardeningTest, huge_toc_size_rejected_without_throwing) {
	const auto file = m_tempDir / "hugetoc.owlpack";
	for (const uint64_t size: {uint64_t{1} << 62U, uint64_t{4} << 30U, ~uint64_t{0}}) {
		writeForged(file, plainPack("ok.txt"), [size](PackHeader& ioHeader) -> void { ioHeader.tocSize = size; });
		std::optional<PackOpenError> error;
		EXPECT_NO_THROW(error = openError(file));
		EXPECT_EQ(error, PackOpenError::TocOutOfBounds);
	}
}

TEST_F(PackHardeningTest, toc_offset_outside_file_rejected) {
	const auto file = m_tempDir / "tocoffset.owlpack";
	writeForged(file, plainPack("ok.txt"), [](PackHeader& ioHeader) -> void { ioHeader.tocOffset = 1ULL << 40U; });
	EXPECT_EQ(openError(file), PackOpenError::TocOutOfBounds);
	writeForged(file, plainPack("ok.txt"), [](PackHeader& ioHeader) -> void { ioHeader.tocOffset = 4; });
	EXPECT_EQ(openError(file), PackOpenError::TocOutOfBounds);
}

TEST_F(PackHardeningTest, huge_declared_toc_original_size_rejected) {
	const auto file = m_tempDir / "hugeorig.owlpack";
	auto pack = plainPack("ok.txt");
	pack.compressed = true;
	pack.dataArea = compressBuffer(pack.dataArea);
	pack.entries[0].dataSize = pack.dataArea.size();
	writeForged(file, pack, [](PackHeader& ioHeader) -> void { ioHeader.tocOriginalSize = 1ULL << 40U; });
	EXPECT_EQ(openError(file), PackOpenError::TocTooLarge);
}

TEST_F(PackHardeningTest, truncated_toc_rejected) {
	const auto file = m_tempDir / "truncated.owlpack";
	PackWriter writer;
	writer.addData({1, 2, 3, 4}, "a.bin", AssetType::Other);
	writer.addData({5, 6, 7, 8}, "b.bin", AssetType::Other);
	ASSERT_TRUE(writer.write(file));
	std::filesystem::resize_file(file, std::filesystem::file_size(file) - 3);
	EXPECT_EQ(openError(file), PackOpenError::TocOutOfBounds);

	auto pack = plainPack("ok.txt");
	pack.entries.push_back(makeEntry("second.txt", sizeof(PackHeader), 7, 7));
	writeForged(file, pack, [](PackHeader& ioHeader) -> void { ioHeader.tocSize -= 5; });
	EXPECT_EQ(openError(file), PackOpenError::TocSizeMismatch);
}

TEST_F(PackHardeningTest, entry_outside_data_area_rejected) {
	const auto file = m_tempDir / "entrybounds.owlpack";
	auto pack = plainPack("ok.txt");
	pack.entries[0].dataOffset = 1ULL << 40U;
	writeForged(file, pack);
	EXPECT_EQ(openError(file), PackOpenError::EntryOutOfBounds);

	pack = plainPack("ok.txt");
	pack.entries[0].dataSize = ~uint64_t{0};
	pack.entries[0].originalSize = ~uint64_t{0};
	writeForged(file, pack);
	EXPECT_EQ(openError(file), PackOpenError::EntryOutOfBounds);

	pack = plainPack("ok.txt");
	pack.entries[0].dataOffset = 0;
	writeForged(file, pack);
	EXPECT_EQ(openError(file), PackOpenError::EntryOutOfBounds);
}

TEST_F(PackHardeningTest, inconsistent_entry_sizes_rejected) {
	const auto file = m_tempDir / "entrysizes.owlpack";
	auto pack = plainPack("ok.txt");
	pack.entries[0].originalSize = 4096;
	writeForged(file, pack);
	EXPECT_EQ(openError(file), PackOpenError::EntrySizeInvalid);

	pack = plainPack("ok.txt");
	pack.compressed = true;
	pack.entries[0].originalSize = g_maxEntrySize + 1;
	writeForged(file, pack);
	EXPECT_EQ(openError(file), PackOpenError::EntrySizeInvalid);

	pack = plainPack("ok.txt");
	pack.compressed = true;
	pack.entries[0].originalSize = 0;
	writeForged(file, pack);
	EXPECT_EQ(openError(file), PackOpenError::EntrySizeInvalid);
}

TEST_F(PackHardeningTest, unknown_asset_type_rejected) {
	const auto file = m_tempDir / "assettype.owlpack";
	auto pack = plainPack("ok.txt");
	pack.entries[0].assetType = static_cast<AssetType>(200);
	writeForged(file, pack);
	EXPECT_EQ(openError(file), PackOpenError::InvalidEntry);
}

TEST_F(PackHardeningTest, wrong_magic_on_valid_pack_rejected) {
	const auto file = m_tempDir / "magic.owlpack";
	writeForged(file, plainPack("ok.txt"),
				[](PackHeader& ioHeader) -> void { ioHeader.magic = std::array<char, 4>{'O', 'W', 'L', 'Q'}; });
	EXPECT_EQ(openError(file), PackOpenError::InvalidMagic);
}

TEST_F(PackHardeningTest, lying_original_size_fails_read_cleanly) {
	const auto file = m_tempDir / "lying.owlpack";
	const std::string content(1000, 'z');
	ForgedPack pack;
	pack.compressed = true;
	pack.dataArea = compressBuffer(std::vector<uint8_t>(content.begin(), content.end()));
	pack.entries.push_back(makeEntry("z.bin", sizeof(PackHeader), pack.dataArea.size(), 900ULL << 20U));
	writeForged(file, pack);
	PackReader reader;
	ASSERT_TRUE(reader.tryOpen(file).has_value());
	EXPECT_FALSE(reader.readEntry("z.bin").has_value());
}

TEST_F(PackHardeningTest, corrupted_block_fails_read_cleanly) {
	const auto file = m_tempDir / "corrupt.owlpack";
	ForgedPack pack;
	pack.compressed = true;
	pack.dataArea = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11};
	pack.entries.push_back(makeEntry("x.bin", sizeof(PackHeader), pack.dataArea.size(), 64));
	writeForged(file, pack);
	PackReader reader;
	ASSERT_TRUE(reader.tryOpen(file).has_value());
	EXPECT_FALSE(reader.readEntry("x.bin").has_value());
}

TEST_F(PackHardeningTest, writer_refuses_unsafe_entries) {
	PackWriter writer;
	writer.addData({1, 2, 3}, "../evil", AssetType::Other);
	EXPECT_FALSE(writer.write(m_tempDir / "refused.owlpack"));
	writer.clear();
	writer.addData({1, 2, 3}, "/abs/evil", AssetType::Other);
	EXPECT_FALSE(writer.write(m_tempDir / "refused.owlpack"));
}

// --- decompressBuffer ----------------------------------------------------------

TEST(PackFormatDecompress, rejects_mismatching_and_oversized_claims) {
	const std::vector<uint8_t> raw(5000, 0x42);
	const auto compressed = compressBuffer(raw);
	EXPECT_EQ(decompressBuffer(compressed, raw.size()), raw);
	EXPECT_TRUE(decompressBuffer(compressed, raw.size() + 1).empty());
	EXPECT_TRUE(decompressBuffer(compressed, raw.size() - 1).empty());
	EXPECT_TRUE(decompressBuffer(compressed, g_maxEntrySize + 1).empty());
	EXPECT_TRUE(decompressBuffer({1, 2, 3, 4, 5, 6, 7, 8}, 16).empty());
	const std::vector truncated(compressed.begin(), compressed.begin() + static_cast<ptrdiff_t>(compressed.size() / 2));
	EXPECT_TRUE(decompressBuffer(truncated, raw.size()).empty());
}

TEST(PackFormatDecompress, frame_without_content_size_is_bounded) {
	const std::vector<uint8_t> raw(300000, 0x17);
	std::vector<uint8_t> compressed(ZSTD_compressBound(raw.size()));
	ZSTD_CCtx* cctx = ZSTD_createCCtx();
	ASSERT_NE(cctx, nullptr);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 0);
	const auto size = ZSTD_compress2(cctx, compressed.data(), compressed.size(), raw.data(), raw.size());
	ZSTD_freeCCtx(cctx);
	ASSERT_EQ(ZSTD_isError(size), 0u);
	compressed.resize(size);
	ASSERT_EQ(ZSTD_getFrameContentSize(compressed.data(), compressed.size()), ZSTD_CONTENTSIZE_UNKNOWN);

	EXPECT_EQ(decompressBuffer(compressed, raw.size()), raw);
	EXPECT_TRUE(decompressBuffer(compressed, raw.size() - 1).empty());
	EXPECT_TRUE(decompressBuffer(compressed, raw.size() + 1).empty());
	EXPECT_TRUE(decompressBuffer(compressed, 900ULL << 20U).empty());
	const std::vector truncated(compressed.begin(), compressed.begin() + static_cast<ptrdiff_t>(compressed.size() / 2));
	EXPECT_TRUE(decompressBuffer(truncated, raw.size()).empty());
}

// --- extraction ----------------------------------------------------------------

TEST_F(PackHardeningTest, resolve_entry_path_confines_to_root) {
	const auto root = m_tempDir / "root";
	const auto inside = resolveEntryPath(root, "textures/hero.png");
	ASSERT_TRUE(inside.has_value());
	EXPECT_EQ(*inside, std::filesystem::weakly_canonical(root) / "textures" / "hero.png");
	EXPECT_FALSE(resolveEntryPath(root, "../outside/evil").has_value());
	EXPECT_FALSE(resolveEntryPath(root, (m_tempDir / "outside" / "evil").string()).has_value());
	EXPECT_FALSE(resolveEntryPath(root, ".").has_value());
}

TEST_F(PackHardeningTest, resolve_entry_path_refuses_symlink_escape) {
	const auto root = m_tempDir / "root";
	std::error_code ec;
	std::filesystem::create_directory_symlink(m_tempDir / "outside", root / "link", ec);
	if (ec)
		GTEST_SKIP() << "Symbolic links unavailable: " << ec.message();
	EXPECT_FALSE(resolveEntryPath(root, "link/evil.txt").has_value());
}

TEST_F(PackHardeningTest, extract_writes_then_skips_up_to_date_entries) {
	const auto file = m_tempDir / "extract.owlpack";
	PackWriter writer;
	writer.addData({1, 2, 3}, "a/one.bin", AssetType::Other);
	writer.addData({4, 5, 6, 7}, "b/c/two.bin", AssetType::Other);
	ASSERT_TRUE(writer.write(file));
	PackReader reader;
	ASSERT_TRUE(reader.tryOpen(file).has_value());

	const auto root = m_tempDir / "root" / "assets";
	const auto first = extractPack(reader, root);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->written, 2u);
	EXPECT_EQ(first->skipped, 0u);
	EXPECT_EQ(std::filesystem::file_size(root / "b" / "c" / "two.bin"), 4u);

	const auto second = extractPack(reader, root);
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->written, 0u);
	EXPECT_EQ(second->skipped, 2u);
}

TEST_F(PackHardeningTest, extract_refuses_symlinked_destination) {
	const auto file = m_tempDir / "symlink.owlpack";
	PackWriter writer;
	writer.addData({1, 2, 3}, "link/evil.txt", AssetType::Other);
	ASSERT_TRUE(writer.write(file));
	const auto root = m_tempDir / "root";
	std::error_code ec;
	std::filesystem::create_directory_symlink(m_tempDir / "outside", root / "link", ec);
	if (ec)
		GTEST_SKIP() << "Symbolic links unavailable: " << ec.message();

	PackReader reader;
	ASSERT_TRUE(reader.tryOpen(file).has_value());
	const auto result = extractPack(reader, root);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), PackExtractError::UnsafeEntryPath);
	EXPECT_FALSE(std::filesystem::exists(m_tempDir / "outside" / "evil.txt"));
}

TEST_F(PackHardeningTest, extract_requires_open_reader) {
	const PackReader reader;
	const auto result = extractPack(reader, m_tempDir / "root");
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), PackExtractError::NotOpen);
}

TEST_F(PackHardeningTest, write_failures_are_reported) {
	const std::vector<uint8_t> data = {1, 2, 3};
	{
		std::ofstream blocker(m_tempDir / "blocker");
		blocker << "x";
	}
	const auto underFile = writeEntryFile(m_tempDir / "blocker" / "child.bin", data);
	ASSERT_FALSE(underFile.has_value());
	EXPECT_EQ(underFile.error(), PackExtractError::CreateDirectoryFailed);

	std::filesystem::create_directories(m_tempDir / "root" / "adir");
	const auto onDirectory = writeEntryFile(m_tempDir / "root" / "adir", data);
	ASSERT_FALSE(onDirectory.has_value());
	EXPECT_EQ(onDirectory.error(), PackExtractError::WriteFailed);

	const auto ok = writeEntryFile(m_tempDir / "root" / "new" / "file.bin", data);
	EXPECT_TRUE(ok.has_value());
	EXPECT_EQ(std::filesystem::file_size(m_tempDir / "root" / "new" / "file.bin"), data.size());
}

TEST_F(PackHardeningTest, extract_stops_on_unwritable_entry) {
	const auto file = m_tempDir / "unwritable.owlpack";
	PackWriter writer;
	writer.addData({1, 2, 3}, "adir", AssetType::Other);
	ASSERT_TRUE(writer.write(file));
	std::filesystem::create_directories(m_tempDir / "root" / "adir");
	PackReader reader;
	ASSERT_TRUE(reader.tryOpen(file).has_value());
	const auto result = extractPack(reader, m_tempDir / "root");
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), PackExtractError::WriteFailed);
}
