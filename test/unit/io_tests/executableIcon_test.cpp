/**
 * @file executableIcon_test.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/assets/pack/ExecutableIcon.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace owl::data::assets::pack;

namespace {

constexpr uint32_t g_textRva = 0x1000;
constexpr uint32_t g_rsrcRva = 0x2000;
constexpr uint32_t g_relocRva = 0x3000;
constexpr size_t g_optionalHeader = 88;
constexpr size_t g_sectionTable = g_optionalHeader + 240;
constexpr size_t g_fileAlignment = 0x200;
constexpr std::array<uint8_t, 8> g_pngSignature = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};

auto u16(const std::span<const uint8_t> iData, const size_t iOffset) -> uint16_t {
	return static_cast<uint16_t>(iData[iOffset] | (iData[iOffset + 1] << 8U));
}

auto u32(const std::span<const uint8_t> iData, const size_t iOffset) -> uint32_t {
	return static_cast<uint32_t>(u16(iData, iOffset)) | (static_cast<uint32_t>(u16(iData, iOffset + 2)) << 16U);
}

void put16(std::vector<uint8_t>& ioData, const size_t iOffset, const uint16_t iValue) {
	ioData[iOffset] = static_cast<uint8_t>(iValue & 0xffU);
	ioData[iOffset + 1] = static_cast<uint8_t>(iValue >> 8U);
}

void put32(std::vector<uint8_t>& ioData, const size_t iOffset, const uint32_t iValue) {
	put16(ioData, iOffset, static_cast<uint16_t>(iValue & 0xffffU));
	put16(ioData, iOffset + 2, static_cast<uint16_t>(iValue >> 16U));
}

auto bytes(const std::string_view iText) -> std::vector<uint8_t> { return {iText.begin(), iText.end()}; }

auto resource(const uint16_t iType, const uint16_t iName, std::vector<uint8_t> iData) -> ExecutableResource {
	return {.type = {.id = iType, .name = {}},
			.name = {.id = iName, .name = {}},
			.language = 0x0409,
			.codePage = 0,
			.data = std::move(iData)};
}

// Hand-written resource tree (numeric ids, one language each, sorted input) for a section at `iRva`.
auto handTree(const std::vector<ExecutableResource>& iResources, const uint32_t iRva) -> std::vector<uint8_t> {
	std::vector<uint16_t> types;
	for (const auto& res: iResources)
		if (types.empty() || types.back() != res.type.id)
			types.push_back(res.type.id);
	const size_t rootSize = 16 + 8 * types.size();
	size_t offset = rootSize;
	std::vector<size_t> typeDirs;
	for (const uint16_t type: types) {
		typeDirs.push_back(offset);
		offset += 16 + 8 * static_cast<size_t>(
								   std::ranges::count_if(iResources, [type](const ExecutableResource& iRes) -> bool {
									   return iRes.type.id == type;
								   }));
	}
	const size_t languageDirs = offset;
	offset += iResources.size() * 24;
	const size_t dataEntries = offset;
	offset += iResources.size() * 16;
	std::vector<size_t> blobs;
	for (const auto& res: iResources) {
		blobs.push_back(offset);
		offset += (res.data.size() + 7) / 8 * 8;
	}
	std::vector<uint8_t> tree(offset, 0);
	put16(tree, 14, static_cast<uint16_t>(types.size()));
	size_t index = 0;
	for (size_t t = 0; t < types.size(); ++t) {
		put32(tree, 16 + 8 * t, types[t]);
		put32(tree, 16 + 8 * t + 4, static_cast<uint32_t>(typeDirs[t]) | 0x80000000U);
		size_t slot = 0;
		for (; index < iResources.size() && iResources[index].type.id == types[t]; ++index, ++slot) {
			const auto& res = iResources[index];
			put16(tree, typeDirs[t] + 14, static_cast<uint16_t>(slot + 1));
			const size_t languageDir = languageDirs + 24 * index;
			put32(tree, typeDirs[t] + 16 + 8 * slot, res.name.id);
			put32(tree, typeDirs[t] + 16 + 8 * slot + 4, static_cast<uint32_t>(languageDir) | 0x80000000U);
			put16(tree, languageDir + 14, 1);
			put32(tree, languageDir + 16, res.language);
			put32(tree, languageDir + 20, static_cast<uint32_t>(dataEntries + 16 * index));
			put32(tree, dataEntries + 16 * index, iRva + static_cast<uint32_t>(blobs[index]));
			put32(tree, dataEntries + 16 * index + 4, static_cast<uint32_t>(res.data.size()));
			std::ranges::copy(res.data, tree.begin() + static_cast<std::ptrdiff_t>(blobs[index]));
		}
	}
	return tree;
}

void putSection(std::vector<uint8_t>& ioPe, const size_t iIndex, const std::string_view iName, const uint32_t iRva,
				const size_t iSize, const size_t iRaw) {
	const size_t header = g_sectionTable + 40 * iIndex;
	std::ranges::copy(iName, ioPe.begin() + static_cast<std::ptrdiff_t>(header));
	put32(ioPe, header + 8, static_cast<uint32_t>(iSize));
	put32(ioPe, header + 12, iRva);
	put32(ioPe, header + 16, static_cast<uint32_t>((iSize + g_fileAlignment - 1) / g_fileAlignment * g_fileAlignment));
	put32(ioPe, header + 20, static_cast<uint32_t>(iRaw));
	put32(ioPe, header + 36, 0x40000040U);
}

// A minimal PE32+ image: `.text`, then `.rsrc` holding `iTree` when not empty, then `.reloc` when asked.
auto makePe(const std::vector<uint8_t>& iTree, const bool iWithReloc) -> std::vector<uint8_t> {
	std::vector<uint8_t> pe(0x400, 0);
	pe[0] = 'M';
	pe[1] = 'Z';
	put32(pe, 0x3c, 64);
	std::ranges::copy(std::string_view{"PE\0\0", 4}, pe.begin() + 64);
	put16(pe, 68, 0x8664);
	put16(pe, 84, 240);
	put16(pe, g_optionalHeader, 0x20b);
	put32(pe, g_optionalHeader + 32, 0x1000);
	put32(pe, g_optionalHeader + 36, static_cast<uint32_t>(g_fileAlignment));
	put32(pe, g_optionalHeader + 60, 0x200);
	put32(pe, g_optionalHeader + 108, 16);
	std::fill_n(pe.begin() + 0x200, 0x200, uint8_t{0xc3});
	uint16_t sections = 1;
	putSection(pe, 0, ".text", g_textRva, 0x200, 0x200);
	uint32_t imageEnd = 0x2000;
	if (!iTree.empty()) {
		const size_t raw = pe.size();
		pe.resize(raw + (iTree.size() + g_fileAlignment - 1) / g_fileAlignment * g_fileAlignment, 0);
		std::ranges::copy(iTree, pe.begin() + static_cast<std::ptrdiff_t>(raw));
		putSection(pe, sections++, ".rsrc", g_rsrcRva, iTree.size(), raw);
		put32(pe, g_optionalHeader + 112 + 16, g_rsrcRva);
		put32(pe, g_optionalHeader + 112 + 20, static_cast<uint32_t>(iTree.size()));
		imageEnd = g_rsrcRva + static_cast<uint32_t>((iTree.size() + 0xfff) / 0x1000 * 0x1000);
	}
	if (iWithReloc) {
		const size_t raw = pe.size();
		pe.resize(raw + g_fileAlignment, 0x5a);
		putSection(pe, sections++, ".reloc", g_relocRva, g_fileAlignment, raw);
		imageEnd = g_relocRva + 0x1000;
	}
	put16(pe, 70, sections);
	put32(pe, g_optionalHeader + 56, imageEnd);
	return pe;
}

auto runnerResources() -> std::vector<ExecutableResource> {
	std::vector<uint8_t> group = {0, 0, 1, 0, 2, 0};
	for (const uint16_t id: {uint16_t{1}, uint16_t{2}}) {
		group.insert(group.end(), {32, 32, 0, 0, 1, 0, 32, 0, 4, 0, 0, 0});
		group.insert(group.end(), {static_cast<uint8_t>(id), 0});
	}
	return {resource(g_resourceTypeIcon, 1, bytes("OLD1")), resource(g_resourceTypeIcon, 2, bytes("OLD2")),
			resource(g_resourceTypeGroupIcon, 1, group), resource(g_resourceTypeVersion, 1, bytes("VERSION-INFO")),
			resource(g_resourceTypeManifest, 1, bytes("<assembly/>"))};
}

auto logo() -> std::vector<uint8_t> {
	std::ifstream in(owl::test::getRootPath() / "engine_assets" / "logo" / "logo_owl_low.png", std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// A 4x2 opaque red image, as a 32-bit BMP.
auto redBitmap() -> std::vector<uint8_t> {
	std::vector<uint8_t> bmp(54 + 4 * 2 * 4, 0);
	bmp[0] = 'B';
	bmp[1] = 'M';
	put32(bmp, 2, static_cast<uint32_t>(bmp.size()));
	put32(bmp, 10, 54);
	put32(bmp, 14, 40);
	put32(bmp, 18, 4);
	put32(bmp, 22, 2);
	put16(bmp, 26, 1);
	put16(bmp, 28, 32);
	for (size_t i = 0; i < 8; ++i) {
		bmp[54 + 4 * i + 2] = 0xff;
		bmp[54 + 4 * i + 3] = 0xff;
	}
	return bmp;
}

auto findResource(const std::vector<ExecutableResource>& iResources, const uint16_t iType, const uint16_t iName)
		-> const ExecutableResource* {
	const auto found = std::ranges::find_if(iResources, [&](const ExecutableResource& iRes) -> bool {
		return iRes.type.name.empty() && iRes.type.id == iType && iRes.name.name.empty() && iRes.name.id == iName;
	});
	return found == iResources.end() ? nullptr : &*found;
}

auto checksum(std::vector<uint8_t> iData) -> uint32_t {
	put32(iData, g_optionalHeader + 64, 0);
	uint64_t sum = 0;
	for (size_t i = 0; i + 1 < iData.size(); i += 2) {
		sum += u16(iData, i);
		sum = (sum & 0xffffU) + (sum >> 16U);
	}
	return static_cast<uint32_t>(((sum & 0xffffU) + (sum >> 16U)) + iData.size());
}

// Checks the icon group of a patched executable and returns its images.
void expectProjectIcon(const std::vector<ExecutableResource>& iResources, const uint16_t iGroupName) {
	const auto* group = findResource(iResources, g_resourceTypeGroupIcon, iGroupName);
	ASSERT_NE(group, nullptr);
	ASSERT_GE(group->data.size(), 6U);
	EXPECT_EQ(u16(group->data, 2), 1);
	const uint16_t count = u16(group->data, 4);
	ASSERT_EQ(count, g_executableIconSizes.size());
	ASSERT_EQ(group->data.size(), 6U + 14U * count);
	EXPECT_EQ(std::ranges::count_if(
					  iResources,
					  [](const ExecutableResource& iRes) -> bool { return iRes.type.id == g_resourceTypeIcon; }),
			  count);
	for (size_t i = 0; i < count; ++i) {
		const size_t entry = 6 + 14 * i;
		const uint16_t side = g_executableIconSizes[i];
		EXPECT_EQ(group->data[entry], side == 256 ? 0 : side);
		EXPECT_EQ(group->data[entry + 1], side == 256 ? 0 : side);
		EXPECT_EQ(u16(group->data, entry + 6), 32);
		const auto* image = findResource(iResources, g_resourceTypeIcon, u16(group->data, entry + 12));
		ASSERT_NE(image, nullptr);
		EXPECT_EQ(image->data.size(), u32(group->data, entry + 8));
		EXPECT_EQ(image->language, group->language);
		if (side == 256) {
			ASSERT_GE(image->data.size(), g_pngSignature.size());
			EXPECT_TRUE(std::ranges::equal(std::span{image->data}.first(8), g_pngSignature));
		} else {
			ASSERT_GE(image->data.size(), 40U);
			EXPECT_EQ(u32(image->data, 0), 40U);
			EXPECT_EQ(u32(image->data, 4), side);
			EXPECT_EQ(u32(image->data, 8), side * 2U);
			EXPECT_EQ(image->data.size(), 40U + side * side * 4U + (side + 31U) / 32U * 4U * side);
		}
	}
}

}// namespace

TEST(ExecutableIcon, ReadsResourceTree) {
	const auto pe = makePe(handTree(runnerResources(), g_rsrcRva), false);
	const auto read = readExecutableResources(pe);
	ASSERT_TRUE(read.has_value());
	const auto expected = runnerResources();
	ASSERT_EQ(read->size(), expected.size());
	for (size_t i = 0; i < expected.size(); ++i) {
		EXPECT_EQ((*read)[i].type, expected[i].type);
		EXPECT_EQ((*read)[i].name, expected[i].name);
		EXPECT_EQ((*read)[i].language, expected[i].language);
		EXPECT_EQ((*read)[i].data, expected[i].data);
	}
}

TEST(ExecutableIcon, NoResourceDirectoryReadsEmpty) {
	const auto read = readExecutableResources(makePe({}, false));
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->empty());
}

TEST(ExecutableIcon, ReplacesIconInPlaceAndKeepsOtherResources) {
	const auto pe = makePe(handTree(runnerResources(), g_rsrcRva), false);
	const auto patched = setExecutableIcon(pe, logo());
	ASSERT_TRUE(patched.has_value());
	EXPECT_EQ(u16(*patched, 70), 2);
	EXPECT_EQ(u32(*patched, g_optionalHeader + 112 + 16), g_rsrcRva);
	EXPECT_EQ(patched->size() % g_fileAlignment, 0U);
	EXPECT_EQ(u32(*patched, g_optionalHeader + 64), checksum(*patched));
	const uint32_t rsrcSize = u32(*patched, g_sectionTable + 40 + 8);
	EXPECT_EQ(u32(*patched, g_optionalHeader + 56), g_rsrcRva + (rsrcSize + 0xfffU) / 0x1000U * 0x1000U);
	const auto read = readExecutableResources(*patched);
	ASSERT_TRUE(read.has_value());
	expectProjectIcon(*read, 1);
	ASSERT_NE(findResource(*read, g_resourceTypeVersion, 1), nullptr);
	EXPECT_EQ(findResource(*read, g_resourceTypeVersion, 1)->data, bytes("VERSION-INFO"));
	ASSERT_NE(findResource(*read, g_resourceTypeManifest, 1), nullptr);
	EXPECT_EQ(findResource(*read, g_resourceTypeManifest, 1)->data, bytes("<assembly/>"));
	EXPECT_TRUE(std::ranges::equal(std::span{*patched}.subspan(0x200, 0x200), std::span{pe}.subspan(0x200, 0x200)));
}

TEST(ExecutableIcon, AppendsSectionWhenResourcesAreNotLast) {
	const auto pe = makePe(handTree(runnerResources(), g_rsrcRva), true);
	const auto patched = setExecutableIcon(pe, logo());
	ASSERT_TRUE(patched.has_value());
	ASSERT_EQ(u16(*patched, 70), 4);
	const size_t newHeader = g_sectionTable + 40 * 3;
	EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(patched->data() + newHeader), 5), ".rsrc");
	EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(patched->data() + g_sectionTable + 40), 6), ".rsrco");
	const uint32_t newRva = u32(*patched, newHeader + 12);
	EXPECT_EQ(newRva, g_relocRva + 0x1000);
	EXPECT_EQ(u32(*patched, g_optionalHeader + 112 + 16), newRva);
	EXPECT_EQ(u32(*patched, g_optionalHeader + 56) % 0x1000, 0U);
	EXPECT_GT(u32(*patched, g_optionalHeader + 56), newRva);
	EXPECT_EQ(u32(*patched, newHeader + 20), pe.size());
	EXPECT_EQ(u32(*patched, g_optionalHeader + 64), checksum(*patched));
	EXPECT_TRUE(std::ranges::equal(std::span{*patched}.first(pe.size()).subspan(g_fileAlignment),
								   std::span{pe}.subspan(g_fileAlignment)));
	const auto read = readExecutableResources(*patched);
	ASSERT_TRUE(read.has_value());
	expectProjectIcon(*read, 1);
	EXPECT_NE(findResource(*read, g_resourceTypeVersion, 1), nullptr);
	EXPECT_NE(findResource(*read, g_resourceTypeManifest, 1), nullptr);
}

TEST(ExecutableIcon, AddsIconToExecutableWithoutResources) {
	const auto patched = setExecutableIcon(makePe({}, true), logo());
	ASSERT_TRUE(patched.has_value());
	const auto read = readExecutableResources(*patched);
	ASSERT_TRUE(read.has_value());
	expectProjectIcon(*read, 1);
	EXPECT_EQ(read->size(), g_executableIconSizes.size() + 1);
	EXPECT_EQ(findResource(*read, g_resourceTypeGroupIcon, 1)->language, 0x0409);
}

TEST(ExecutableIcon, KeepsIconsOfOtherGroupsAndNamedResources) {
	auto resources = runnerResources();
	resources.push_back(resource(g_resourceTypeIcon, 7, bytes("OTHER")));
	resources.push_back({.type = {.id = 0, .name = u"PNG"},
						 .name = {.id = 0, .name = u"SPLASH"},
						 .language = 0,
						 .codePage = 1252,
						 .data = bytes("splash")});
	const auto written = writeExecutableResources(makePe({}, false), resources);
	ASSERT_TRUE(written.has_value());
	const auto patched = setExecutableIcon(*written, logo());
	ASSERT_TRUE(patched.has_value());
	const auto read = readExecutableResources(*patched);
	ASSERT_TRUE(read.has_value());
	ASSERT_FALSE(read->empty());
	EXPECT_EQ(read->front().type.name, u"PNG");
	EXPECT_EQ(read->front().name.name, u"SPLASH");
	EXPECT_EQ(read->front().codePage, 1252U);
	EXPECT_EQ(read->front().data, bytes("splash"));
	ASSERT_NE(findResource(*read, g_resourceTypeIcon, 7), nullptr);
	EXPECT_EQ(findResource(*read, g_resourceTypeIcon, 7)->data, bytes("OTHER"));
	const auto* group = findResource(*read, g_resourceTypeGroupIcon, 1);
	ASSERT_NE(group, nullptr);
	std::set<uint16_t> ids;
	for (size_t i = 0; i < u16(group->data, 4); ++i) ids.insert(u16(group->data, 6 + 14 * i + 12));
	EXPECT_FALSE(ids.contains(7));
	EXPECT_EQ(ids.size(), g_executableIconSizes.size());
}

TEST(ExecutableIcon, FitsNonSquareImageWithTransparentPadding) {
	const auto images = encodeIconImages(redBitmap());
	ASSERT_TRUE(images.has_value());
	ASSERT_EQ(images->size(), g_executableIconSizes.size());
	const auto& small = images->front();
	const auto pixel = [&small](const size_t iX, const size_t iY) -> std::span<const uint8_t> {
		return std::span{small}.subspan(40 + ((15 - iY) * 16 + iX) * 4, 4);
	};
	EXPECT_EQ(pixel(8, 0)[3], 0);
	EXPECT_EQ(pixel(8, 15)[3], 0);
	EXPECT_EQ(pixel(8, 8)[2], 0xff);
	EXPECT_EQ(pixel(8, 8)[0], 0);
	EXPECT_EQ(pixel(8, 8)[3], 0xff);
	const size_t mask = 40 + 16 * 16 * 4;
	EXPECT_EQ(small[mask + 15 * 4], 0xff);
	EXPECT_EQ(small[mask + 8 * 4], 0x00);
}

TEST(ExecutableIcon, RefusesInvalidInput) {
	EXPECT_EQ(readExecutableResources(bytes("#!/bin/sh\necho not a PE\n")).error(), ExecutableIconError::NotExecutable);
	auto pe = makePe(handTree(runnerResources(), g_rsrcRva), false);
	auto noPe = pe;
	noPe[64] = 'X';
	EXPECT_EQ(readExecutableResources(noPe).error(), ExecutableIconError::NotExecutable);
	EXPECT_EQ(readExecutableResources(std::span{pe}.first(200)).error(), ExecutableIconError::Corrupted);
	auto loop = pe;
	put32(loop, 0x400 + 20, 0x80000000U);
	EXPECT_EQ(readExecutableResources(loop).error(), ExecutableIconError::Corrupted);
	auto outside = pe;
	put32(outside, g_sectionTable + 40 + 20, 0x100000);
	EXPECT_EQ(readExecutableResources(outside).error(), ExecutableIconError::Corrupted);
	auto badData = pe;
	put32(badData, g_optionalHeader + 112 + 16, 0x9000);
	EXPECT_EQ(readExecutableResources(badData).error(), ExecutableIconError::Corrupted);
	auto signedPe = pe;
	put32(signedPe, g_optionalHeader + 112 + 36, 0x100);
	EXPECT_EQ(setExecutableIcon(signedPe, logo()).error(), ExecutableIconError::Signed);
	EXPECT_EQ(setExecutableIcon(pe, bytes("not an image")).error(), ExecutableIconError::ImageDecode);
	auto full = makePe({}, true);
	put32(full, g_optionalHeader + 60, static_cast<uint32_t>(g_sectionTable + 40 * 2));
	EXPECT_EQ(setExecutableIcon(full, logo()).error(), ExecutableIconError::NoHeaderRoom);
}

TEST(ExecutableIcon, PatchesFileInPlace) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_executable_icon";
	std::filesystem::create_directories(dir);
	const auto exe = dir / "Game.exe";
	const auto pe = makePe(handTree(runnerResources(), g_rsrcRva), true);
	{
		std::ofstream out(exe, std::ios::binary);
		out.write(reinterpret_cast<const char*>(pe.data()), static_cast<std::streamsize>(pe.size()));
	}
	const auto icon = owl::test::getRootPath() / "engine_assets" / "logo" / "logo_owl_low.png";
	ASSERT_TRUE(setExecutableIcon(exe, icon).has_value());
	std::ifstream in(exe, std::ios::binary);
	const std::vector<uint8_t> patched{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	const auto read = readExecutableResources(patched);
	ASSERT_TRUE(read.has_value());
	expectProjectIcon(*read, 1);
	EXPECT_EQ(setExecutableIcon(dir / "missing.exe", icon).error(), ExecutableIconError::ReadFailed);
	EXPECT_EQ(setExecutableIcon(icon, icon).error(), ExecutableIconError::NotExecutable);
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

TEST(ExecutableIcon, ErrorMessages) {
	for (const auto error:
		 {ExecutableIconError::ReadFailed, ExecutableIconError::NotExecutable, ExecutableIconError::Corrupted,
		  ExecutableIconError::Signed, ExecutableIconError::NoHeaderRoom, ExecutableIconError::ImageDecode,
		  ExecutableIconError::ImageEncode, ExecutableIconError::WriteFailed})
		EXPECT_FALSE(getErrorMessage(error).empty());
}
