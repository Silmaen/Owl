/**
 * @file FontCharset_test.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Tests of the charset summary and of the sample layout of the font viewer.
 */

#include "document/viewer/FontCharset.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

using namespace owl;
using namespace owl::nest::viewer;
using data::fonts::CodePointRange;
using data::fonts::FontSettings;

namespace {
auto totalOf(const CharsetSummary& iSummary) -> size_t {
	size_t total = 0;
	for (const auto& block: iSummary.blocks) total += block.count;
	return total;
}
}// namespace

TEST(FontCharset, namesTheUnicodeBlocks) {
	EXPECT_EQ(unicodeBlockName(U'A'), "Basic Latin");
	EXPECT_EQ(unicodeBlockName(0x00E9), "Latin-1 Supplement");
	EXPECT_EQ(unicodeBlockName(0x20AC), "Currency Symbols");
	EXPECT_EQ(unicodeBlockName(0x0416), "Cyrillic");
	EXPECT_EQ(unicodeBlockName(0x1F600), "Other");
}

TEST(FontCharset, summarizesTheDefaultCharset) {
	const auto summary = summarizeCharset({});
	ASSERT_FALSE(summary.blocks.empty());
	EXPECT_EQ(summary.codePoints, totalOf(summary));
	EXPECT_EQ(summary.blocks.front().name, "Basic Latin");
	EXPECT_TRUE(std::ranges::is_sorted(summary.blocks, [](const CharsetBlock& iLhs, const CharsetBlock& iRhs) -> bool {
		return iLhs.range.first < iRhs.range.first;
	}));
}

TEST(FontCharset, extraRangesAddTheirCodePointsOnce) {
	const auto base = summarizeCharset({});
	FontSettings settings;
	settings.extraRanges = {{.first = 0x0400, .last = 0x04FF}, {.first = 0x0400, .last = 0x047F}};
	const auto summary = summarizeCharset(settings);
	EXPECT_EQ(summary.codePoints, base.codePoints + 256);
	const auto cyrillic = std::ranges::find(summary.blocks, "Cyrillic", &CharsetBlock::name);
	ASSERT_NE(cyrillic, summary.blocks.end());
	EXPECT_EQ(cyrillic->count, 256u);
}

TEST(FontCharset, cutsARangeAtTheBlockBounds) {
	FontSettings settings;
	settings.extraRanges = {{.first = 0x0370, .last = 0x0450}};
	const auto summary = summarizeCharset(settings);
	const auto greek = std::ranges::find(summary.blocks, "Greek and Coptic", &CharsetBlock::name);
	const auto cyrillic = std::ranges::find(summary.blocks, "Cyrillic", &CharsetBlock::name);
	ASSERT_NE(greek, summary.blocks.end());
	ASSERT_NE(cyrillic, summary.blocks.end());
	EXPECT_EQ(greek->count, 0x90u);
	EXPECT_EQ(cyrillic->range, (CodePointRange{.first = 0x0400, .last = 0x0450}));
}

TEST(FontCharset, keepsCodePointsOutsideTheTableInOther) {
	FontSettings settings;
	settings.extraRanges = {{.first = 0x1F600, .last = 0x1F60F}};
	const auto summary = summarizeCharset(settings);
	const auto other = std::ranges::find(summary.blocks, "Other", &CharsetBlock::name);
	ASSERT_NE(other, summary.blocks.end());
	EXPECT_EQ(other->count, 16u);
	EXPECT_EQ(summary.codePoints, totalOf(summary));
}

TEST(FontCharset, stacksTheSampleRows) {
	constexpr std::array<float, 3> sizes{10.f, 20.f, 40.f};
	const auto rows = layoutSampleRows(sizes, 1.f, 0.f, 4.f);
	ASSERT_EQ(rows.size(), 3u);
	EXPECT_FLOAT_EQ(rows[0].top, 4.f);
	EXPECT_FLOAT_EQ(rows[0].height, 10.f);
	EXPECT_FLOAT_EQ(rows[1].top, 18.f);
	EXPECT_FLOAT_EQ(rows[2].top, 42.f);
	EXPECT_FLOAT_EQ(rows[2].height, 40.f);
	const auto padded = layoutSampleRows(sizes, 1.2f, 0.1f, 0.f);
	EXPECT_FLOAT_EQ(padded[0].height, 14.f);
	EXPECT_TRUE(layoutSampleRows({}, 1.f, 0.f, 4.f).empty());
}
